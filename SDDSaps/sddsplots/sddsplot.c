/************************************************************************* \
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplot.c
 * 
 * plotting program for SDDS files
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.150  2011/03/22 23:47:10  borland
 Added thickness qualifier for arrows.

 Revision 1.149  2011/03/18 18:15:20  borland
 Added gpng (gigantic PNG) type.

 Revision 1.148  2011/01/11 22:51:03  soliday
 Changed all the strcpy commands to strcpy_ss because of problems with
 RedHat Enterprise 6. If a strcpy copies the result to the same memory
 space you will get unexpected results.

 Revision 1.147  2010/09/14 16:20:11  soliday
 Added the rspectral order for the colors. This is the reverse of the spectral
 order.

 Revision 1.146  2010/07/08 21:26:47  borland
 Added "first" qualifier to -omnipresent option.  When given, omnipresent datasets
 are plotted first.  The default is to plot them last.

 Revision 1.145  2010/06/03 16:33:43  borland
 Added fixForRequest qualifier to -graphic option.

 Revision 1.144  2010/01/05 20:06:45  soliday
 Added the -intensityBar option which can be used to move the intensity bar
 and to resize the label text to the left and the unit text above.

 Revision 1.143  2009/07/28 14:01:16  borland
 Added scroll feature to -replicate option.  Greatly improved memory management for
 split and replicated datasets.  Added -nocolorbar option.

 Revision 1.142  2009/07/09 21:48:29  borland
 Added scroll qualifier to -replicate option help.

 Revision 1.141  2009/06/17 21:40:19  borland
 Added fixForFile qualifier to -graphic option.  Allows using syntax like
 -column=x,y <filenames> -graph=line,vary,fixforfile -split=page and getting
 fixed line type for data from each file.

 Revision 1.140  2009/04/29 14:15:59  borland
 Added nosubtype qualifier to -legend option, to allow suppressing the subtype in the legend.

 Revision 1.139  2009/03/02 15:04:40  borland
 Updated version number.

 Revision 1.138  2009/03/02 15:04:09  borland
 Updated usage message.

 Revision 1.137  2008/08/20 18:37:46  soliday
 Updated so that when plotting multiple pages based on the -split columnBin
 option, it will now locate the maximum value for all the panels and the
 intensity bar will be identical for each panel. Before the maximum on the
 intensity bar on a panel was independent of the other panels.

 Revision 1.136  2008/05/23 20:39:25  soliday
 Added the ability to use the graphic options eachPage and eachRequest with
 the graphic option fixForName.

 Revision 1.135  2007/11/02 21:35:06  soliday
 Fixed some issues with the -split option and multiple files and the intensity bar.

 Revision 1.134  2007/07/05 15:00:04  soliday
 Fixed an issue when the spectral color option is used as a global option.

 Revision 1.133  2007/05/17 19:40:32  soliday
 Modified to allow changing the thickness for the intensity bar labels.

 Revision 1.132  2007/05/10 20:16:18  soliday
 Fixed a memory freeing issue.

 Revision 1.131  2007/04/20 19:39:33  borland
 Fixed serious bugs with enumerated value plotting.
 Changed order of routines in the main() procedure so that assign_enumerated_values() gets
 run properly.
 Fixed the sorting algorithm in assign_enumerated_values() so that it assigns unique values
 for unique strings, without messing up the order from the data file.

 Revision 1.130  2007/04/10 16:26:22  soliday
 Fixed a problem where it was allocating the wrong number of colors for the
 intensity bar.

 Revision 1.129  2007/03/08 18:54:39  soliday
 Added the ability to vary both the type and subtype at the same time.

 Revision 1.128  2006/10/20 14:57:57  jiaox
 1) Added format key word for -topline and -legend options.If given with paramteter keyword (or @), the format will be applied to the parameter output. If not given, but there is format_string for the parameter in
 the SDDS file, it output will take that format_string for output.

 2) Fixed a problem that when multiple file with multiple pages are plotted with -split=page -order=Spectral and eachpage keyword is used in -graphic option, only part of the spectra is actually used in the plot.

 Revision 1.127  2006/08/21 21:58:52  jiaox
 Fixed the bug that the -paramemterChange with -split option did not display the proper color codings.

 Revision 1.126  2006/03/21 17:52:00  soliday
 Fixed problems with the intesity bar with ceps and png output. Also modified
 the split by column data option to automatically find the minimum value
 for a plot request and use that as the start value if no start value is given.
 This enables multiple plots to have the same scale even if they are in
 different parts of the range.

 Revision 1.125  2006/03/20 23:09:06  soliday
 Xuesong improved the zooming feature. I improved the color spectrum
 intensity bar when using multiple panels and spliting the data based
 on column data.

 Revision 1.124  2006/02/22 14:59:36  jiaox
 Added a feature that parse the sddsplot commandline to mpl_ motif driver for
 improved zoom.

 Revision 1.123  2005/12/07 22:01:11  jiaox
 Made the line types ( thickness color dash style) consistent among motif,
 postscript and png devices. Added device arguments dashes and linetypetable
 for these devices to draw lines with user defined line styles.

 Revision 1.122  2005/11/07 22:20:55  soliday
 Updated to remove compiler warnings on Linux.

 Revision 1.121  2005/11/04 22:46:45  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.120  2005/10/04 22:04:30  borland
 Fixed usage message for -timeFilter option.

 Revision 1.119  2005/08/29 21:31:24  shang
 added linetype to xlabel, ylabel, topline and title options

 Revision 1.118  2005/08/19 19:05:17  borland
 Added fill qualifier for -graphic option to the usage message.

 Revision 1.117  2005/08/19 13:14:38  borland
 Fixed some code style problems in previous changes.

 Revision 1.116  2005/08/15 14:11:28  jiaox
 Added keyword "fill" for graphic option. Also added circel type(9) for symbols. Fixed the bug that caused incorrect plotting if verticalprint is
 used.

 Revision 1.115  2005/07/15 18:29:07  soliday
 Fixed problem with intensity bar when the split command includes the
 start option.

 Revision 1.114  2005/05/24 15:58:43  soliday
 Commented out problematic free statement.

 Revision 1.113  2005/05/06 19:25:29  soliday
 Fixed issue with spectral colors when connecting symbols.

 Revision 1.112  2005/04/28 16:31:34  soliday
 Added the ability to plot with an intensity bar that is split based on
 parameter data.

 Revision 1.111  2005/04/13 18:09:56  soliday
 Fixed an issue with the scale of the intensity bar when there are multiple
 plot panels which should have different ranges.

 Revision 1.110  2005/03/22 21:20:12  shang
 modified to plot intensity bar only when split column is given

 Revision 1.109  2005/03/22 03:33:54  soliday
 Made various changes including fixing a problem with the intensity bar
 colors. Also changed png and ps default colors so they are all
 the same as motif.

 Revision 1.108  2005/03/07 22:42:03  shang
 added plotting the color bar for -order option

 Revision 1.107  2005/02/22 17:51:06  shang
 added feature for plotting labels of sub-tick for log-scale axis and added
 line type option to pointLabel to plot the point label with specified color

 Revision 1.106  2005/01/22 22:35:25  borland
 Added thickness qualifier to pointLabel option.

 Revision 1.105  2005/01/05 18:28:09  borland
 Added usage entry for the -dither option.

 Revision 1.104  2004/08/02 18:43:28  borland
 Added -dither option.  It can be used to make scatter plots look more like
 density plots, particularly for digitized data.

 Revision 1.103  2004/03/30 00:07:03  shang
 added add_plot_requests() back in ErasePlotSpecification(PLOT_SPEC *plspec) function,
 which was commented out before and caused the failure of movie plots

 Revision 1.102  2004/03/26 19:48:11  shang
 added -xExclude, -yExclude, -toPage and -fromPage options

 Revision 1.101  2003/04/14 14:46:38  shang
 added -timeFilter feature

 Revision 1.100  2002/12/18 17:49:23  borland
 The "noData" string is now dynamically allocated to avoid segmentation
 faults on Linux.

 Revision 1.99  2002/12/06 19:44:07  borland
 Updated usage message to reflect changes to -factor.

 Revision 1.98  2002/12/04 00:34:49  borland
 Added fractional-deviation mode to -mode option:
 -mode=[xy]FractionalDeviation
 Added value-from-file and invert features to -factor option:
 -factor=[xy]Parameter=<name>,[xy]Invert
 Fixed invert feature for -offset option (was ignored when the offset
 was given literally but used when given as parameter).

 Revision 1.97  2002/11/14 03:03:15  borland
 To fix problems with building mpl_motif:
 Moved SDDS_ReadLineColorTable from sddsplot.c to graphics.c.
 Moved LINE_COLOR_TABLE structure definition from sddsplot.h to
 graphics.h.

 Revision 1.96  2002/11/13 19:19:09  borland
 Added line-color-table option for PNG devices.  Permits changing the
 color and number of lines.  Overrides the -ordercolors option.

 Revision 1.95  2002/09/11 15:56:20  soliday
 Fixed a bug where it was possible that the badScale array would not be
 initialized.

 Revision 1.94  2002/08/14 17:24:53  soliday
 Added Open License

 Revision 1.93  2002/07/03 15:29:27  shang
 changed the -tagrequest=<number> by -tagrequest=<value> (tag could be any string)

 Revision 1.92  2002/01/10 02:29:46  borland
 Added -thickness option, which permits specifying the default thickness
 for all other options (expect -graphic).  Also, fixed bug in thickness
 control for the -xlabel (etc.) options (a flag was reused).

 Revision 1.91  2002/01/10 02:02:16  borland
 Added -rowlimit option to force program to ignore pages with more than a
 defined number of rows.

 Revision 1.90  2002/01/09 23:39:49  borland
 Added line thickness setting for enumerated scales.

 Revision 1.89  2001/10/12 18:27:43  soliday
 Added the -nextPage option to split requests.

 Revision 1.88  2001/10/09 21:30:26  soliday
 Added the thickness option to the -string option.

 Revision 1.87  2001/09/20 18:26:42  soliday
 Vertical enum labels ignore the allowed space because it is invalid.

 Revision 1.86  2001/09/20 16:27:13  soliday
 Fixed some additional problems with the scale factor for enum.

 Revision 1.85  2001/09/20 15:50:02  soliday
 The enum scale option overrides the autoscaleing feature.

 Revision 1.84  2001/08/13 20:11:52  soliday
 Added thickness options for several options.

 Revision 1.83  2001/01/24 16:25:07  soliday
 Standardized usage message.

 Revision 1.82  2000/09/22 20:49:50  soliday
 Avoided a problem with attempting to free dataset->label.
 This causes a larger memory leak but until I can pinpoint
 the problem, this will have to do.

 Revision 1.81  2000/09/14 17:20:50  soliday
 Fixed tick modulus problem.

 Revision 1.80  2000/07/17 16:24:33  borland
 Removed debugging statements.

 Revision 1.79  2000/07/17 16:23:58  borland
 Fixed bug that resulted from modifying the string passed to draw_sddsplot_strings().

 Revision 1.78  2000/07/07 15:59:41  soliday
 Fixed -drawLine usage message.

 Revision 1.77  2000/06/29 14:54:29  borland
 Restored function of -noscales option; however, it still won't work properly
 for multiple scales.  Fixed usage message for -[xy]scalesgroup options.

 Revision 1.76  2000/06/09 20:56:36  soliday
 -orderColors is no longer a global option.

 Revision 1.75  2000/06/08 17:38:41  soliday
 Added spectral colors.

 Revision 1.74  2000/06/07 20:22:18  soliday
 Fixed -orderColors usage message.

 Revision 1.73  2000/06/07 20:14:09  soliday
 Added option for custom colors.

 Revision 1.72  2000/06/02 20:57:29  soliday
 Added thickness option to lineTypeDefault, drawLine, axes, tick and subtick
 options.

 Revision 1.71  2000/05/11 20:18:08  soliday
 Added line thickness to legend.

 Revision 1.70  2000/05/02 18:40:37  borland
 Fixed a egregious bug in alignZeros (argument parsing) and added some diagnostics.

 Revision 1.69  2000/04/27 19:29:48  soliday
 Added support for line thickness.

 Revision 1.68  2000/04/17 19:01:54  soliday
 Now works with Borland C.

 Revision 1.67  2000/02/10 17:47:19  borland
 Connect feature now works for more graphic types.

 Revision 1.66  2000/01/10 19:03:59  borland
 Improved function of -alignZero option.

 Revision 1.65  1999/11/30 22:48:06  borland
 Added fixForName qualifier to -graph option.  Allows fixing the line/symbol/etc
 type for a yname.  Fixed bug in legend creation (was duplicating the same
 legend in cases where the legend string had special sequences).

 Revision 1.64  1999/11/16 22:28:21  soliday
 Commented out four free statements that were not operating as expected.

 Revision 1.63  1999/11/09 15:04:11  borland
 Added clip qualifier to -drawLines option so that lines needn't be shown
 if outside the pspace.

 Revision 1.62  1999/10/24 19:04:57  borland
 Added usage for -repeat option.

 Revision 1.61  1999/10/06 20:00:59  borland
 Changed the order of column-binning and sparse/sample/clip operations
 in main loop, so that -clip and -mode=[xy]=offset have the expected
 behavior.

 Revision 1.60  1999/09/18 01:03:55  borland
 Fixed the size of error bars shown in legends.

 Revision 1.59  1999/08/17 20:31:15  borland
 Added checkseconds and timeout qualifiers to -repeat option.

 Revision 1.58  1999/07/22 18:35:26  soliday
 Added WIN32 support

 Revision 1.57  1999/07/21 18:07:25  borland
 Eliminated many memory leaks.  Program is now suitable for running with
 -repeat option, although not all leaks are gone.

 Revision 1.56  1999/07/14 15:15:13  borland
 Added repeat-mode plotting, in which the data files are periodically checked
 and a new plot is generated when the files change.

 Revision 1.55  1999/06/29 15:47:15  borland
 Changed return value for multicommand_AP from 0 to 1.  Previously returned
 nothing, then was given 0 return in error.

 Revision 1.54  1999/06/03 16:08:46  soliday
 Removed compiler warnings under linux.

 Revision 1.53  1999/05/24 18:17:24  borland
 Now support the -ticksettings=[xy]scalechars qualifier for time mode ticks.

 Revision 1.52  1999/05/24 17:51:28  borland
 Fixed some label scaling bugs and reinstated the -labelsize option.

 Revision 1.51  1999/05/18 20:34:48  borland
 Improved handling of x label position when pspace is modified.

 Revision 1.50  1999/05/03 15:51:15  borland
 Fixed bugs with subticks: no linetype control, no grid control.

 Revision 1.49  1999/02/10 14:27:00  borland
 Store the string "$i$iNo data$d$d" in an array so that there are no problems
 with gnu compiler when the string is passed to jxyplot.
 Also, set the character size to the default for each panel using new
 set_default_char_size() routine.

 Revision 1.48  1999/01/06 20:27:38  borland
 Fixed the version number in the usage message.

 Revision 1.47  1998/12/22 16:02:39  borland
 New character scaling algorithm for all labels.  Considerable changes
 to psymbol routine that draws characters and returns the size of
 strings.

 Revision 1.46  1998/10/01 22:48:05  borland
 Reimplemented scale and offset qualifiers for -xlabel and -ylabel.
 Implemented {xy}scaleChars qualifier for -ticksettings.
 These options allow users to control the size of characters on scale labels
 and ticks labels.

 Revision 1.45  1998/10/01 21:30:17  borland
 Fixed code that does axes so it doesn't draw them when they are outside the
 users plotting region.

 Revision 1.44  1998/09/08 18:12:29  borland
 Added slant qualifier to -strings option.

 Revision 1.43  1998/09/02 14:09:03  borland
 Added -alignZero option.  Also added delay qualifier to -zoom option.
 Changed implementation of panel scale determination.  Also modified
 arguments to make_scale() to permit inverted scales.

 Revision 1.42  1998/08/25 15:05:31  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.41  1998/02/20 20:11:55  borland
 Added qalign and palign qualifiers for overlays.  Results in lining up
 of 0 of overlay data with 0 of pspace.

 Revision 1.40  1998/02/20 15:19:28  borland
 Improved/fixed overlays.  Now has qoffset/poffset option.   xoffset/yoffset
 no longer changes behavior from unit to normal mode.  May break some applications,
 but they'll be easy to fix.

 Revision 1.39  1998/02/18 22:39:21  borland
 Fixed bug that prevented -overlay request from being executed when
 unit mode was requested in both planes and no other flags where given.

 Revision 1.38  1997/03/04 19:06:30  borland
 Added proper linetype for gapped lines for symbol plotting with connected points.

 Revision 1.37  1997/02/07 17:34:25  borland
 add_filenames() now checks new filenames against the list of filenames for
 a request.  Duplicates are ignored.

 Revision 1.36  1997/02/05 16:21:33  borland
 Fixed usage message for -drawLine

 Revision 1.35  1997/02/05 15:59:08  borland
 Added -drawline option.

 Revision 1.34  1996/11/19 00:15:53  borland
 Fixed usage message, which was goofed up by reformatting of the code.

 Revision 1.33  1996/11/11 21:31:55  borland
 Added control over the type of labels for logscales: -tickSettings option
 now takes [xy]nonExponentialLabels options.

 Revision 1.32  1996/11/08 17:15:18  borland
 Added -replicate to usage message.

 Revision 1.31  1996/11/08 16:54:51  borland
 Implemented -replicate option.

 Revision 1.30  1996/09/30 17:50:43  borland
 Fixed some problems with _echo printouts (testing for NULL pointer before
 printing).  Added show_filters().

 Revision 1.29  1996/09/30 15:56:30  borland
 Added some casts in memcpy and before malloc calls to avoid waring messages
 on SUN4.

 Revision 1.28  1996/09/30 15:40:43  borland
 Fixed problem with copying of data from plot request 0; now allocates new
 memory to avoid realloc problem.  Fixed a few problems with printing
 of null strings in _echo output.

 Revision 1.27  1996/08/26 16:54:54  borland
 Fixed statements that adjust vertical pspace range for -toptitle mode.
 Adjustment is now scaled to existing range.

 * Revision 1.26  1996/04/30  21:06:26  borland
 * Updated usage messages.
 *
 * Revision 1.25  1996/04/22  16:39:16  borland
 * Fixed usage message portion for -separate option.
 *
 * Revision 1.24  1996/04/10  21:36:57  borland
 * Implemented -pointLabel option for labeling data points with string values
 * from data files.
 *
 * Revision 1.23  1996/03/21  17:19:42  borland
 * Removed dependence on SDDS library (gif.trm graphics.c).
 * Added presparse option and implementation.
 *
 * Revision 1.22  1996/03/21  15:32:24  borland
 * Moved device argument routines from sddsplot.c to graphics.c; added prototypes.
 *
 * Revision 1.21  1996/03/20  18:52:06  borland
 * Added improved device argument support via setDeviceArgv and getDeviceArgv
 * routines.  Backward compatible for x11 and mif devices.  Added generation
 * file feature to gif driver for multipage output.
 *
 * Revision 1.20  1996/02/29  05:42:36  borland
 * Added variable dot size using subtype qualifier of -graphic option.
 * Required adding an extra argument to the plot_dots() routine.
 *
 * Revision 1.19  1996/02/21  15:59:53  borland
 * Improved functioning of previous mods to plot_sddsplot_data, so that it
 * works right even if one of the scales is valid.
 *
 * Revision 1.18  1996/02/21  15:37:34  borland
 * Modifed plot_sddsplot_data() to better handle the case of no data or undefined
 * limits for a panel; it now makes a border with labels, but writes "No data"
 * into the center.
 *
 * Revision 1.17  1996/02/16  17:17:02  borland
 * Fixed incorrect usage message for -string option.
 *
 * Revision 1.16  1996/02/09  00:07:26  borland
 * Added xtime and ytime qualifiers to -ticksettings option, along with code
 * to do time-labeled axes (assumes time values are times in seconds since
 * epoch).
 *
 * Revision 1.15  1996/01/21  17:57:38  borland
 * Added -namescan option to control scanning of files for matches to wildcard
 * dataname specifications; this allows the user to cut running time when there
 * are many files with identical data elements.
 *
 * Revision 1.14  1996/01/17  23:48:33  borland
 * Added loop to check for multiple enumerates in the same position, so labels
 * don't get overwritten and so label scale is right.
 *
 * Revision 1.13  1996/01/16  18:49:10  borland
 * Fixed problem in -limit=autoscaling when data was transformed (have to
 * transform limits also).  Fixed bugs in enumerate scale routines that
 * resulted in misplaced ticks; fixed autoscaling of y enumerated labels.
 *
 * Revision 1.12  1995/12/27  20:17:26  borland
 * Made a minor change to the usage message.
 *
 * Revision 1.11  1995/12/13  21:59:54  borland
 * Updated help text for -limit and -range options.
 *
 * Revision 1.10  1995/12/09  03:28:16  borland
 * Changed handling of enumerated data so that column-binning will work without
 * mixing up the data order from page to page.
 *
 * Revision 1.9  1995/12/02  03:25:10  borland
 * Added support for -multicommand option; allows running sddsplot in a mode
 * wherein it accepts multiple command completions from the standard input.
 *
 * Revision 1.8  1995/11/13  15:58:55  borland
 * Added long cast to strlen() in comparison to satisfy Solaris compiler.
 *
 * Revision 1.7  1995/11/03  23:03:10  borland
 * Modified routines for adding dataset slots to realloc in big chunks, to
 * avoid SUN realloc problem that lead to N^2 growth in memory usage; added
 * debugging statements to data reading routine to track memory usage.
 *
 * Revision 1.6  1995/10/18  21:56:31  borland
 * Modified data-reading routines to read multiple instances of the same file
 * only once (sddsplotRead.c).  Other routines have minor modifications.
 *
 * Revision 1.5  1995/10/11  00:18:52  borland
 * Added code to support new -ytopline option.
 *
 * Revision 1.4  1995/09/25  16:39:18  borland
 * Changed adjust_sddsplot_overlay_data to return 0/1 if overlay not possible/
 * possible.  Added warning message to sparse_sample_clip if all data is removed
 * by winnowing.
 *
 * Revision 1.3  1995/09/14  22:04:25  borland
 * Added support for yimpulse and ybar graphic types.
 *
 * Revision 1.2  1995/09/05  21:09:40  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "graph.h"
#include "table.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>
#include "graphics.h"

#if defined(_WIN32)
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#ifdef COMPILE_AS_SUBROUTINE
void putenv_label(char *name, char *value);
extern FILE *outfile;
#else
FILE *outfile;
#endif

#define TITLE_LEN 132

extern void shorten_filename(char *s);
extern void cleanup_label(char *s);

#ifdef MOTIF_DEVICE
#define DEFAULT_DEVICE "motif"
#else
#define DEFAULT_DEVICE "qt"
#endif

#define DEFAULT_FONT "rowmans"

char *USAGE1 = "sddsplot \n\
 [-columnnames=<x-name>,<y-name-list>[,<y1-name-list>|<x1-name>,<y1-name-list>] | \n\
 [-xExclude=<x-name>[,<x-name-list]] [-yExclude=<y-name>[,<y-name-list>]] \n\
  -parameternames=<x-name>,<y-name-list>[,<y1-name-list>|<x1-name>,<y1-name-list>] | \n\
  -arraynames=<x-name>,<y-name-list>[,<y1-name-list>|<x1-name>,<y1-name-list>] | \n\
  -mplfiles | -keep[={names|files}]]\n\
  [-toPage=<pagenumber>] [-fromPage=<pagenumber>] \n\
  [<filename>...]\n\
  -repeat[=checkSeconds=<number>][,timeout=<seconds>]\n\
  -namescan={all|first}\n\
  -lineTypeDefault=<integer>[,thickness=<value>] [-thickness=<integer>] \n\
  -device=<device-name>[,<device-args>] [-listDevices]\n\
  -font=<font-name> -listFonts\n\
  -fixfontsize=[all=.02][,legend=.015][,<x|y>xlabel=<value>][,<x|y>ticks=<value>][,title=<value>][,topline=<value>]\n\
  -output=<filename>\n\
  -graphic=<element>[,type=<type>][,fill][,subtype={<type>|type}][,thickness=<integer>][,connect[={<linetype>|type|subtype}]][,vary[={type|subtype}][,scale=<factor>][,eachFile][,eachPage][,eachRequest][,fixForName][,fixForFile]]\n\
  -arrowSettings=[scale=<value>][,barbLength=<value>][,barbAngle=<deg>][,lineType=<number>][,centered][,cartesianData][,polarData][,scalarData][,singleBarb][,autoscale]\n\
  -scales=<xlo>,<xhi>,<ylo>,<yhi>\n\
  -pspace=<plo>,<phi>,<qlo>,<qhi>\n\
  -lspace=<plo>,<phi>,<qlo>,<qhi>\n\
  -limit=[{xy}Minimum=<value>][,{xy}Maximum=<value>][,autoscaling]\n\
  -range=[{x|y}{minimum|maximum|center}=<value>]\n\
  -zoom=[xFactor=<value>][,yFactor=<value>][,{xCenter=<value>|pCenter=<value>}][,{yCenter=<value>|qCenter=<value>}][,delay]\n\
  -alignZero[={xcenter|xfactor|pPin=<value>}][,{ycenter|yfactor|qPin=<value>}]\n\
  -aspectratio=<value>\n\
  -unsuppressZero[={x|y}]\n\
  -layout=<nx>,<ny>[,limitPerPage=<integer>]\n\
  -sameScale[={x|y}][,global]\n\
  -joinScale={x|y}\n\
  -sever[={xGap=<value>|yGap=<value>}]\n\
  -separate[=[<number-to-group>][,groupsOf=<number>][,fileIndex][,fileString][,nameIndex][,nameString][,page][,request][,tag][,subpage]\n\
  -omnipresent[=first]\n\
  -replicate={number=<integer>|match={pages|requests|files|names}}[,scroll]\n\
  -groupBy=[=request][,tag][,fileIndex][,nameIndex][,page][subpage][,fileString][,nameString]\n";

char *USAGE2 = "  -tagRequest={<value>|@<parameter-name>}\n\
  -xLabel=[@<parameter-name>|<string>|use={name|symbol|description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer]\n\
  -yLabel=[@<parameter-name>|<string>|use={name|symbol|description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>] \n\
  -title=[@<parameter-name>|<string>|use={name|symbol|description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>] \n\
  -topline=[@<parameter-name>|<string>|use={name|symbol|description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>] \n\
  -labelSize=<fraction>\n\
  -string={@<parameter-name>|<string>},{xCoordinate=<value|@<parameterName>>|pCoordinate=<value|@<parameterName>>},{yCoordinate=<value|@<parameterName>>|qCoordinate=<value|@<parameterName>>}[,scale=<value>][,angle=<degrees>][,edit=<edit-command>][,justify=<mode>][,slant=<degrees>][,linetype=<linetype>][,thickness=<integer>]\n\
  -drawLine={x0value=<value>|p0value=<value>|x0parameter=<name>|p0parameter=<name>},{x1value=<value>|p1value=<value>|x1parameter=<name>|p1parameter=<name>},{y0value=<value>|q0value=<value>|y0parameter=<name>|q0parameter=<name>},{y1value=<value>|q1value=<value>|y1parameter=<name>|q1parameter=<name>}[,linetype=<integer>][,thickness=<integer>][,clip]\n\
  [-filenamesOnTopline | -yTopline]\n";

char *USAGE3 = "  -topTitle\n\
  -noLabels\n\
  -noScales[={x|y}]\n\
  -noBorder\n\
  -axes[=x][,y][,linetype=<number>][,thickness=<number>]\n\
  -grid[={x|y}] [-dataBehind]\n\
  -subticksettings=[{xy}divisions=<integer>][,[{xy}]grid][,[{xy}]linetype=<integer>][,[{xy}]thickness=<integer>][,{xy}size=<fraction>][,xNoLogLabel][,yNoLogLabel]\n\
  -ticksettings=[{xy}spacing=<value>][,[{xy}]grid][,[{xy}]linetype=<integer>][,[{xy}]thickness=<integer>][,{xy}size=<fraction>][{xy}modulus=<value>][,[{xy}]logarithmic][,{xy}factor=<value>][,{xy}time][,{xy}nonExponentialLabels][,{xy}scaleChars=<factor>][,[{xy}]labelThickness=<integer>]\n\
  -enumeratedScales=[interval=<integer>][,limit=<integer>][,scale=<factor>][,allTicks][,rotate][,editCommand=<string>]\n\
  -xScalesGroup={ID=<string>|fileIndex|fileString|nameIndex|nameString|page|request|units}\n";

char *USAGE4 = "  -yScalesGroup={ID=<string>|fileIndex|fileString|nameIndex|nameString|page|request|units}\n\
  -legend={{xy}symbol|{xy}description|{xy}name|filename|specified=<string>|parameter=<name>}{,editCommand=<edit-string>}[,units][,firstFileOnly][,scale=<value>][,thickness=<integer>][,nosubtype]\n\
  -pointlabel=<name>[,edit=<editCommand>][,scale=<number>][,justifyMode={rcl}{bct}][,thickness=<integer>[,lineType=<integer>]][,vertical]\n\
  -newPanel  -endPanel  -nextPage\n\
  -split={parameterChange=<parameter-name>[,width=<value>][,offset=<value>]|columnbin=<column-name},width=<value>[,offset=<value>][,completely]|pages[,interval=<interval>]}\n\
  -transpose \n\
  -mode=[{x|y}={linear|log|autolog|specialScales|normalize|offset|center|meanCenter|fractionalDeviation}][,...]\n\
  -stagger=[{xy}Increment=<value>][,files][,datanames]\n\
  -factor=[{xy}Multiplier=<value>][,{xy}Parameter=<value>][,{xy}Invert]\n";

char *USAGE5 = "  -offset=[{x|y}change={value>][,{x|y}parameter=<name>][,{x|y}invert][,{x|y}beforelog]\n \
  -dither=[{x|y}range=<fraction>]\n\
  -overlay=[{xy}Mode=<mode>][,{xy}Factor=<value>][,{xy}Offset=<value>][,{xy}Center]\n\
  -sample=<fraction>  -sparse=<interval>[,<offset>]  -presparse=<interval>[,<offset>]\n\
  -clip=<head>,<tail>[,invert] -rowlimit=<integer>\n\
  -match={column|parameter},<match-test>[,<match-test>[,<logic-operation>...]\n\
  -filter={column|parameter},<range-spec>[,<range-spec>[,<logic-operation>...]\n\
  -timeFilter={column|parameter},<name>[,before=YYYY/MM/DD@HH:MM:SS][,after=YYYY/MM/DD@HH:MM:SS][,invert]\n\
  -intensityBar=[text=<string>,][labelsize=<value>][,unitsize=<value>][,xadjust=<value>]\n\
  -convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]\n\
  -orderColors={temperature|rtemperature|spectral|rspectral|start=(<red>,<green>,<blue>){[,finish=(<red>,<green>,<blue>)]|[,increment=(<red>,<green>,<blue>)]}}\n\
  All colors range from 0 to 65535.\n\
  -device={qt|motif|png|postscript}[,dashes,linetypetable=<lineDefineFile>]\n\
  for qt and motif device arguments, use '-dashes 1 -linetype lineDefineFile' \n\
  for png device arguments, use 'rootname=<name>,template=<string>,onwhite,onblack,dashes,movie' \n\n\
  A <match-test> is of the form <name>=<matching-string>[,!], where ! signifies logical negation.\n\
  A <logic-operation> is one of & (logical and) or | (logical or), optionally followed by a ! to \n\
  logically negate the value of the expression.\n\
  A <range-spec> is of the form <name>,<lower-value>,<upper-value>[,!].\n\
  <element> is one of continue, line, symbol, errorbar, impulse, yimpulse, dot, bar, or ybar\n\
   \nProgram by Michael Borland.  (This is version 11, March 2009)\n";

/* still to implement:
   -graphic=[parameter=<name-template>],[columntype=<column-name>],[parametertype=<parameter-name>] ?
   -graphic=<whatever>,match=<wildcardString>
   -layout=<nx>,<ny>[,pattern=<pattern>]    e.g., -layout=3,3,pattern=111.011.001
   -linetypedefault=[symbols=<type>][,text=<type>][,subticks=<type>][,ticks=<type>]
   -draw 
   array data
   better control of character size for legends
*/

static long main_keywords;
static char *main_keyword[] = {
  "aspectratio",
  "device",
  "graphic",
  "lspace",
  "mplfiles",
  "outputfile",
  "columnnames",
  "parameternames",
  "arraynames",
  "pspace",
  "scales",
  "unsuppresszero",
  "zoom",
  "nolabels",
  "noborder",
  "noscales",
  "xlabel",
  "ylabel",
  "title",
  "topline",
  "string",
  "filenamesontopline",
  "verticalprint",
  "toptitle",
  "datestamp",
  "samescale",
  "legend",
  "overlay",
  "separate",
  "newpanel",
  "omnipresent",
  "layout",
  "split",
  "grid",
  "axes",
  "subticksettings",
  "mode",
  "stagger",
  "sever",
  "sparse",
  "sample",
  "clip",
  "keep",
  "filter",
  "timeFilter",
  "match",
  "drawline",
  "swap",
  "_showlinkdate",
  "transpose",
  "linetypedefault",
  "ticksettings",
  "labelsize",
  "enumeratedscales",
  "arrowsettings",
  "groupby",
  "tagrequest",
  "endpanel",
  "listdevices",
  "factor",
  "limit",
  "_echo",
  "offset",
  "ytopline",
  "multicommand",
  "range",
  "namescan",
  "presparse",
  "pointlabel",
  "replicate",
  "xscalesgroup",
  "yscalesgroup",
  "alignzero",
  "repeat",
  "ordercolors",
  "nextpage",
  "rowlimit",
  "thickness",
  "xExclude",
  "yExclude",
  "toPage",
  "fromPage",
  "dither",
  "intensityBar",
  "databehind",
  "joinscale",
  "font",
  "listfonts",
  "fixfontsize",
  "convertunits",
  NULL,
};

extern long aspectratio_AP(PLOT_SPEC *plotspec, char **item, long items),
  device_AP(PLOT_SPEC *plotspec, char **item, long items),
  graphic_AP(PLOT_SPEC *plotspec, char **item, long items),
  lspace_AP(PLOT_SPEC *plotspec, char **item, long items),
  mplfiles_AP(PLOT_SPEC *plotspec, char **item, long items),
  outputfile_AP(PLOT_SPEC *plotspec, char **item, long items),
  columnnames_AP(PLOT_SPEC *plotspec, char **item, long items),
  parameternames_AP(PLOT_SPEC *plotspec, char **item, long items),
  arraynames_AP(PLOT_SPEC *plotspec, char **item, long items),
  pspace_AP(PLOT_SPEC *plotspec, char **item, long items),
  scales_AP(PLOT_SPEC *plotspec, char **item, long items),
  unsuppresszero_AP(PLOT_SPEC *plotspec, char **item, long items),
  zoom_AP(PLOT_SPEC *plotspec, char **item, long items),
  nolabels_AP(PLOT_SPEC *plotspec, char **item, long items),
  noborder_AP(PLOT_SPEC *plotspec, char **item, long items),
  noscales_AP(PLOT_SPEC *plotspec, char **item, long items),
  xlabel_AP(PLOT_SPEC *plotspec, char **item, long items),
  ylabel_AP(PLOT_SPEC *plotspec, char **item, long items),
  title_AP(PLOT_SPEC *plotspec, char **item, long items),
  topline_AP(PLOT_SPEC *plotspec, char **item, long items),
  string_AP(PLOT_SPEC *plotspec, char **item, long items),
  filenamesontopline_AP(PLOT_SPEC *plotspec, char **item, long items),
  verticalprint_AP(PLOT_SPEC *plotspec, char **item, long items),
  toptitle_AP(PLOT_SPEC *plotspec, char **item, long items),
  datestamp_AP(PLOT_SPEC *plotspec, char **item, long items),
  samescale_AP(PLOT_SPEC *plotspec, char **item, long items),
  legend_AP(PLOT_SPEC *plotspec, char **item, long items),
  overlay_AP(PLOT_SPEC *plotspec, char **item, long items),
  separate_AP(PLOT_SPEC *plotspec, char **item, long items),
  newpanel_AP(PLOT_SPEC *plotspec, char **item, long items),
  omnipresent_AP(PLOT_SPEC *plotspec, char **item, long items),
  layout_AP(PLOT_SPEC *plotspec, char **item, long items),
  split_AP(PLOT_SPEC *plotspec, char **item, long items),
  grid_AP(PLOT_SPEC *plotspec, char **item, long items),
  axes_AP(PLOT_SPEC *plotspec, char **item, long items),
  subticks_AP(PLOT_SPEC *plotspec, char **item, long items),
  mode_AP(PLOT_SPEC *plotspec, char **item, long items),
  stagger_AP(PLOT_SPEC *plotspec, char **item, long items),
  sever_AP(PLOT_SPEC *plotspec, char **item, long items),
  sparse_AP(PLOT_SPEC *plotspec, char **item, long items),
  sample_AP(PLOT_SPEC *plotspec, char **item, long items),
  clip_AP(PLOT_SPEC *plotspec, char **item, long items),
  keep_AP(PLOT_SPEC *plotspec, char **item, long items),
  filter_AP(PLOT_SPEC *plotspec, char **item, long items),
  time_filter_AP(PLOT_SPEC *plotspec, char **item, long items),
  match_AP(PLOT_SPEC *plotspec, char **item, long items),
  drawline_AP(PLOT_SPEC *plotspec, char **item, long items),
  swap_AP(PLOT_SPEC *plotspec, char **item, long items),
  showlinkdate_AP(PLOT_SPEC *plotspec, char **item, long items),
  transpose_AP(PLOT_SPEC *plotspec, char **item, long items),
  linetypedefault_AP(PLOT_SPEC *plotspec, char **item, long items),
  ticksettings_AP(PLOT_SPEC *plotspec, char **item, long items),
  labelsize_AP(PLOT_SPEC *plotspec, char **item, long items),
  enumeratedscales_AP(PLOT_SPEC *plotspec, char **item, long items),
  arrowsettings_AP(PLOT_SPEC *plotspec, char **item, long items),
  groupby_AP(PLOT_SPEC *plotspec, char **item, long items),
  tagrequest_AP(PLOT_SPEC *plotspec, char **item, long items),
  endpanel_AP(PLOT_SPEC *plotspec, char **item, long items),
  listDevices_AP(PLOT_SPEC *plotspec, char **item, long items),
  factor_AP(PLOT_SPEC *plotspec, char **item, long items),
  limit_AP(PLOT_SPEC *plotspec, char **item, long items),
  argEcho_AP(PLOT_SPEC *plotspec, char **item, long items),
  offset_AP(PLOT_SPEC *plotspec, char **item, long items),
  ylabelontopline_AP(PLOT_SPEC *plotspec, char **item, long items),
  multicommand_AP(PLOT_SPEC *plotspec, char **item, long items),
  range_AP(PLOT_SPEC *plotspec, char **item, long items),
  namescan_AP(PLOT_SPEC *plotspec, char **item, long items),
  presparse_AP(PLOT_SPEC *plotspec, char **item, long items),
  pointlabel_AP(PLOT_SPEC *plotspec, char **item, long items),
  replicate_AP(PLOT_SPEC *plotspec, char **item, long items),
  xScalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items),
  yScalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items),
  alignzero_AP(PLOT_SPEC *plotspec, char **item, long items),
  repeatMode_AP(PLOT_SPEC *plotspec, char **item, long items),
  orderColors_AP(PLOT_SPEC *plotspec, char **item, long items),
  nextpage_AP(PLOT_SPEC *plotspec, char **item, long items),
  rowlimit_AP(PLOT_SPEC *plotspec, char **item, long items),
  thickness_AP(PLOT_SPEC *plotspec, char **item, long items),
  xexclude_columnnames_AP(PLOT_SPEC *plotspec, char **item, long items),
  yexclude_columnnames_AP(PLOT_SPEC *plotspec, char **item, long items),
  toPage_AP(PLOT_SPEC *plotspec, char **item, long items),
  fromPage_AP(PLOT_SPEC *plotspec, char **item, long items),
  dither_AP(PLOT_SPEC *plotspec, char **item, long items),
  intensityBar_AP(PLOT_SPEC *plotspec, char **item, long items),
  dataBehind_AP(PLOT_SPEC *plotspec, char **item, long items),
  joinscale_AP(PLOT_SPEC *plotspec, char **item, long items),
  font_AP(PLOT_SPEC *plotspec, char **item, long items),
  listFonts_AP(PLOT_SPEC *plotspec, char **item, long items),
  fixfontsize_AP(PLOT_SPEC *plotspec, char **item, long items),
  convertUnits_AP(PLOT_SPEC *plotspec, char **item, long items);

static long (*main_parser[])(PLOT_SPEC *plotspec, char **item, long items) = {
  aspectratio_AP,
  device_AP,
  graphic_AP,
  lspace_AP,
  mplfiles_AP,
  outputfile_AP,
  columnnames_AP,
  parameternames_AP,
  arraynames_AP,
  pspace_AP,
  scales_AP,
  unsuppresszero_AP,
  zoom_AP,
  nolabels_AP,
  noborder_AP,
  noscales_AP,
  xlabel_AP,
  ylabel_AP,
  title_AP,
  topline_AP,
  string_AP,
  filenamesontopline_AP,
  verticalprint_AP,
  toptitle_AP,
  datestamp_AP,
  samescale_AP,
  legend_AP,
  overlay_AP,
  separate_AP,
  newpanel_AP,
  omnipresent_AP,
  layout_AP,
  split_AP,
  grid_AP,
  axes_AP,
  subticks_AP,
  mode_AP,
  stagger_AP,
  sever_AP,
  sparse_AP,
  sample_AP,
  clip_AP,
  keep_AP,
  filter_AP,
  time_filter_AP,
  match_AP,
  drawline_AP,
  swap_AP,
  showlinkdate_AP,
  transpose_AP,
  linetypedefault_AP,
  ticksettings_AP,
  labelsize_AP,
  enumeratedscales_AP,
  arrowsettings_AP,
  groupby_AP,
  tagrequest_AP,
  endpanel_AP,
  listDevices_AP,
  factor_AP,
  limit_AP,
  argEcho_AP,
  offset_AP,
  ylabelontopline_AP,
  multicommand_AP,
  range_AP,
  namescan_AP,
  presparse_AP,
  pointlabel_AP,
  replicate_AP,
  xScalesGroup_AP,
  yScalesGroup_AP,
  alignzero_AP,
  repeatMode_AP,
  orderColors_AP,
  nextpage_AP,
  rowlimit_AP,
  thickness_AP,
  xexclude_columnnames_AP,
  yexclude_columnnames_AP,
  toPage_AP,
  fromPage_AP,
  dither_AP,
  intensityBar_AP,
  dataBehind_AP,
  joinscale_AP,
  font_AP,
  listFonts_AP,
  fixfontsize_AP,
  convertUnits_AP,
  NULL,
};

extern void parseCommandlineToMotif(int argc, char **argv);
extern void parseCommandlineToQT(int argc, char **argv);
extern void passCommandlineToPNG(int argc, char **argv);
extern void passCommandlineToPS(int argc, char **argv);

static long argEcho = 0;
static short multicommandMode = 0, repeatMode = 0;
static int32_t repeatModeCheckInterval = 1, repeatModeTimeout = 900;

extern int term;
int dataBehind = 0;

void show_filter(FILE *fp, FILTER_DEFINITION *filter);
void show_time_filter(FILE *fp, TIME_FILTER_DEFINITION *timeFilter);

void ErasePlotSpecification(PLOT_SPEC *plspec);
long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc);
void waitForFilesToUpdate(PLOT_SPEC *plspec, long firstCall, long interval, long timeout);
void EraseScannedArgs(SCANNED_ARG *scanned, long argc);

#ifdef COMPILE_AS_SUBROUTINE
int sddsplot_main(int commandlineArgc, char **commandlineArgv)
#else
  int main(int commandlineArgc, char **commandlineArgv)
#endif
{
  SCANNED_ARG *scanned; /* structure for scanned arguments */
  long i_arg, i, kw, first;
  short deviceInitialized, filesInitialized;
  int argc;
  PLOT_SPEC plot_spec;
  char *device, **argv;
  long repeats = 0;

#ifndef COMPILE_AS_SUBROUTINE
  {
    int n;
    int X11_args();
    n = X11_args(commandlineArgc, commandlineArgv);
    commandlineArgc -= n;
    commandlineArgv += n;
  }
#endif

  main_keywords = 0;
  while (main_keyword[main_keywords])
    {
      if (!main_parser[main_keywords])
        {
          bomb("programming error--number of main_keywords doesn't match number of main_parsers", NULL);
          exit(1);
        }
      main_keywords++;
    }
  if (main_parser[main_keywords])
    {
      bomb("programming error--number of main_keywords doesn't match number of main_parsers", NULL);
      exit(1);
    }

  if (commandlineArgc < 2)
    {
      void put_link_date();
      fprintf(stderr, "%s%s%s%s%s", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5);
      put_link_date(stderr);
      /*        list_terms(stderr); */
      exit(1);
    }

  SDDS_ZeroMemory((void *)&plot_spec, sizeof(plot_spec));
  add_plot_request(&plot_spec);

  plot_spec.fontsize[0].autosize = 1;

  parseCommandlineToMotif(commandlineArgc, commandlineArgv);
  parseCommandlineToQT(commandlineArgc, commandlineArgv);
  passCommandlineToPNG(commandlineArgc, commandlineArgv);
  passCommandlineToPS(commandlineArgc, commandlineArgv);
  argc = commandlineArgc;
  argv = tmalloc(sizeof(*argv) * argc);
  SDDS_CopyStringArray(argv, commandlineArgv, commandlineArgc);
  first = 1;
  deviceInitialized = 0;
  filesInitialized = 0;
  do
    {
      if (!first && repeatMode)
        {
          EraseScannedArgs(scanned, argc);
          free(scanned);
        }
      argc = scanargsg(&scanned, argc, argv);
      argc = add_sddsfile_arguments(&scanned, argc);
      for (i_arg = 1; i_arg < argc; i_arg++)
        {
          if (scanned[i_arg].arg_type == OPTION)
            {
              /* process options here */
              for (i = 0; i < scanned[i_arg].n_items; i++)
                interpret_escapes(scanned[i_arg].list[i]);
              if (argEcho)
                {
                  long j;
                  printf("%s: ", scanned[i_arg].list[0]);
                  for (j = 1; j < scanned[i_arg].n_items; j++)
                    printf("%s ", scanned[i_arg].list[j]);
                  putchar('\n');
                }
              if ((kw = match_string(scanned[i_arg].list[0], main_keyword, main_keywords, 0)) < 0)
                {
                  fprintf(stderr, "error: unknown keyword: %s\n", scanned[i_arg].list[0]);
                  exit(1);
                }
              else if (!(*main_parser[kw])(&plot_spec, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1))
                exit(1);
            }
          else
            {
              if (argEcho)
                printf("file: %s\n", scanned[i_arg].list[0]);
              if (!add_filename(&plot_spec, scanned[i_arg].list[0]))
                exit(1);
            }
        }

      if (first)
        {
#ifndef COMPILE_AS_SUBROUTINE
          outfile = plot_spec.outputfile ? fopen_e(plot_spec.outputfile, "w", 0) : stdout;
#endif

#ifndef COMPILE_AS_SUBROUTINE
          if (!plot_spec.device && ((device = getenv("SDDS_DEVICE")) || (device = getenv("MPL_DEVICE"))))
            {
#  ifdef VAX_VMS
              str_tolower(device);
#  endif
              SDDS_CopyString(&plot_spec.device, device);
            }
#endif
          if (!plot_spec.device)
            SDDS_CopyString(&plot_spec.device, DEFAULT_DEVICE);

          if (plot_spec.deviceArgc)
            {
              gs_device_arguments(plot_spec.deviceArgv[0], 0); /* for backward compatibility */
              setDeviceArgv(plot_spec.deviceArgv, plot_spec.deviceArgc);
            }
          if (!plot_spec.font)
            {
              SDDS_CopyString(&plot_spec.font, DEFAULT_FONT);
            }
          set_default_font(plot_spec.font);
        }

      if (multicommandMode && first)
        {
          first = 0;
          ErasePlotSpecification(&plot_spec);
          ReadMoreArguments(&argv, &argc, commandlineArgv, commandlineArgc);
          continue;
        }
      first = 0;

      if (plot_spec.plot_requests == 1)
        bomb("no plot requests", NULL);

      /* Scan each request to expand wildcarded names by looking in the data files. 
       * Sets the xname, yname, x1name, y1name, was_wildname, and datanames entries in
       * the plot request structures 
       */
      resolve_wildcard_datanames(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   resolve_wildcard_datanames(&plot_spec)\n", memory_count());
#endif

      if (argEcho)
        {
          fprintf(stderr, "Showing plot specifications:\n");
          show_plot_spec(&plot_spec, stderr);
          for (i = 0; i < plot_spec.plot_requests; i++)
            show_plot_request(plot_spec.plot_request, i, stderr);
        }

      /* Swaps x and y names in the plot request structures, as requested. */
      perform_dataname_swapping(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   perform_dataname_swapping(&plot_spec)\n", memory_count());
#endif

      /* Reads data named in plot request structures.  Puts this into the PLOT_DATA structures
       * in plot_spec.  Takes care of splitting data based on parameters or pages.
       * Also resolves any requests for information from paramateres for use
       * in labels or tags.
       */
      read_sddsplot_data(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   read_sddsplot_data(&plot_spec)\n", memory_count());
#endif

      /* Does sparsing, sampling, and clipping of data sets based on plot requests 
       * Note that presparsing is done in read_sddsplot_data.
       */
      sparse_sample_clip(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   sparse_sample_clip(&plot_spec)\n", memory_count());
#endif

      /* Splits datasets according to any -split requests in the plot requests.
       * If splits are done, ends up making new data sets in the plot spec structure. 
       */
      columnbin_sddsplot_data(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   columnbin_sddsplot_data(&plot_spec)\n", memory_count());
#endif

      /* Implements the -transpose option on all the datasets under an applicable plot
       * request.  Ends up mixing data between those sets sets (rotation of columns into
       * rows).  Also composes a custom topline label to show the value of the original
       * x column for each new data set.  Composes y label if needed. 
       */
      perform_dataset_transpositions(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   perform_dataset_transpositions(&plot_spec)\n", memory_count());
#endif
      if (plot_spec.datasets == 0) {
        if (repeatMode) {
          printf("warning: no datasets to plot\n");
          fflush(stdout);
          sleep(1);
          continue;
        }
        bomb("no datasets to plot", NULL);
      }
      /* Checks for proper data for arrow plotting.  Then does offseting and (if requested)
       * autoscaling of x1 and y1 data.
       */
      perform_arrow_autoscaling(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   perform_arrow_autoscaling(&plot_spec);\n", memory_count());
#endif

      /* Copies specified data sets.  Provides a way to plot the same dataset in several
       * contexts without duplicating plot requests, for example.
       */
      replicate_datasets(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   replicate_datasets(&plot_spec)\n", memory_count());
#endif

      /* Determines how many plot panels to create and which data sets go on each. */
      determine_panel_assignments(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   determine_panel_assignments(&plot_spec)\n", memory_count());
#endif

      /* Determines what graphical element to use for each data set, based on what the plot
       * request specified and what order the set is on its panel.
       */
      determine_graphic_assignments(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   determine_graphic_assignments(&plot_spec)\n", memory_count());
#endif

      /* Assigns scales group ID's to any datasets that are without one and makes an
       * index for use in collecting data for scales groups later (in determine_limits)
       */
      assign_scales_groups(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   assign_scales_groups(&plot_spec)\n", memory_count());
#endif

      /* Does log, offset, normalization, multiplication by a factor, and staggering. */
      perform_dataset_conversions(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   perform_dataset_conversions(&plot_spec)\n", memory_count());
#endif

      /* For data sets where x or y is an enumerated value, sets the x or y array to
       * integer values (spaced by 1) that are used when the data is plotted.
       */
      assign_enumerate_values(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   assign_enumerate_values(&plot_spec)\n", memory_count());
#endif

      /* Determines the limits in user's coordinates for each scale group on each panel. */
      determine_limits(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   determine_limits(&plot_spec)\n", memory_count());
#endif

      /* Determines the size of the pspace for each panel.  Must accomodate legends, if requested
       * as well as the number of y and x scales. 
       */
      determine_panel_pspaces(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   determine_panel_pspaces(&plot_spec)\n", memory_count());
#endif

      /* Determines the x, y, title, and topline labels for each panel.  Uses the labels given
       * by the user for the relevant requests, if any.  Also sets up some structures for
       * use in making scales.
       */
      determine_labels(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   determine_labels(&plot_spec)\n", memory_count());
#endif

      /* Resolves problems with 'omnipresent' datasets which don't get set up with the rest
       */
      resolve_omnipresent_references(&plot_spec);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   resolve_omnipresent_references(&plot_spec)\n", memory_count());
#endif

      if (argEcho)
        {
          fprintf(stderr, "Showing plot specifications:\n");
          show_plot_spec(&plot_spec, stderr);
          for (i = 0; i < plot_spec.plot_requests; i++)
            show_plot_request(plot_spec.plot_request, i, stderr);
        }

      if (repeatMode && !filesInitialized)
        {
          filesInitialized = 1;
          waitForFilesToUpdate(&plot_spec, 1, repeatModeCheckInterval, repeatModeTimeout);
        }
      plot_sddsplot_data(&plot_spec, !deviceInitialized);
#ifdef DEBUG
      fprintf(stderr, "memory: %10ld  after   plot_sddsplot_data(&plot_spec, !deviceInitialized)\n", memory_count());
#endif
      deviceInitialized = 1;

      if (multicommandMode)
        {
          /* erase old plot information */
          ErasePlotSpecification(&plot_spec);
          if (!ReadMoreArguments(&argv, &argc, commandlineArgv, commandlineArgc))
            break;
        }
      else if (repeatMode)
        {
          ErasePlotSpecification(&plot_spec);
          waitForFilesToUpdate(&plot_spec, 0, repeatModeCheckInterval, repeatModeTimeout);
          repeats--;
        }
      else
        break;
      if (repeats == 0)
        break;
    }
  while (1);
  ErasePlotSpecification(&plot_spec);
  return (0);
}

#undef DEBUG

#include <sys/types.h>
#include <sys/stat.h>

void waitForFilesToUpdate(PLOT_SPEC *plspec, long firstCall, long interval, long timeout)
{
  static time_t *modTime;
  static char **filename;
  static long nfiles;
  char *filenameCopy;
  struct stat statbuf;
  long ireq, ifile, changed;
  int32_t duplicate;
  long timeLeft;

  if (firstCall)
    {
      nfiles = 0;
      for (ireq = 0; ireq < plspec->plot_requests; ireq++)
        nfiles += plspec->plot_request[ireq].filenames;
      if (!(modTime = malloc(sizeof(*modTime) * nfiles)) ||
          !(filename = malloc(sizeof(*filename) * nfiles)))
        bomb("memory allocation failure (waitForFilesToUpdate)", NULL);
      nfiles = 0;
      for (ireq = 0; ireq < plspec->plot_requests; ireq++)
        for (ifile = 0; ifile < plspec->plot_request[ireq].filenames; ifile++)
          {
            SDDS_CopyString(&filenameCopy, plspec->plot_request[ireq].filename[ifile]);
            binaryInsert((void **)filename,
                         nfiles, filenameCopy,
                         (int (*)(const void *, const void *))strcmp, &duplicate);
            if (!duplicate)
              nfiles++;
          }
      for (ifile = 0; ifile < nfiles; ifile++)
        {
          if (stat(filename[ifile], &statbuf))
            {
              perror("problem with file");
              exit(1);
            }
          modTime[ifile] = statbuf.st_mtime;
        }
    }
  else
    {
      sleep(interval);
      timeLeft = timeout - interval;
      changed = 0;
      while (!changed && timeLeft > 0)
        {
          for (ifile = 0; ifile < nfiles; ifile++)
            {
              if (stat(filename[ifile], &statbuf))
                {
                  sleep(interval);
                  timeLeft -= interval;
                  ifile--;
                  continue;
                }
              if (statbuf.st_mtime > modTime[ifile])
                changed = 1;
              modTime[ifile] = statbuf.st_mtime;
            }
          if (!changed)
            {
              sleep(interval);
              timeLeft -= interval;
            }
        }
      if (timeLeft <= 0)
        exit(0);
    }
}

void determine_graphic_assignments(PLOT_SPEC *plspec)
{
  long set, request, lastpanel, lastreq, panel;
  char **yName = NULL;
  long yNames = 0, index;
  char **filename = NULL;
  long filenames = 0;
  GRAPHIC_SPEC **referenceGraphic = NULL;
  GRAPHIC_SPEC **referenceGraphicFN = NULL;

  lastpanel = lastreq = -1;
  for (set = 0; set < plspec->datasets; set++)
    {
      panel = plspec->dataset[set].plotpanel;
      request = plspec->dataset[set].request_index;
      if (plspec->plot_request[request].graphic.element == -1)
        {
          if (panel != lastpanel || request <= 1)
            {
              fprintf(stderr, "error: graphic.element==-1 with panel=%ld, last panel=%ld, and request=%ld\n",
                      panel, lastpanel, request);
              exit(1);
            }
          plspec->plot_request[request].graphic.element = plspec->plot_request[request - 1].graphic.element;
          plspec->plot_request[request].graphic.arrow = plspec->plot_request[request - 1].graphic.arrow;
          plspec->plot_request[request].graphic.vary = plspec->plot_request[request - 1].graphic.vary;
          plspec->plot_request[request].graphic.type = plspec->plot_request[request - 1].graphic.type +
            (plspec->plot_request[request].graphic.vary ? 1 : 0);
          plspec->plot_request[request].graphic.subtype = plspec->plot_request[request - 1].graphic.subtype;
          plspec->plot_request[request].graphic.thickness = plspec->plot_request[request - 1].graphic.thickness;
          plspec->plot_request[request].graphic.scale = plspec->plot_request[request - 1].graphic.scale;
          plspec->plot_request[request].graphic.connect_linetype =
            plspec->plot_request[request - 1].graphic.connect_linetype;
          plspec->plot_request[request].graphic.fill = plspec->plot_request[request - 1].graphic.fill;
        }
      plspec->dataset[set].graphic.element = plspec->plot_request[request].graphic.element;
      plspec->dataset[set].graphic.arrow = plspec->plot_request[request].graphic.arrow;
      if (plspec->dataset[set].graphic.element == PLOT_ARROW && (!plspec->dataset[set].y1 || (!(plspec->dataset[set].graphic.arrow.flags & ARROW_SCALAR_DATA) && !plspec->dataset[set].x1)))
        bomb("arrow plotting requested with insufficient data specified", NULL);
      plspec->dataset[set].graphic.type = plspec->plot_request[request].graphic.type;
      plspec->dataset[set].graphic.flags = plspec->plot_request[request].graphic.flags;
      plspec->dataset[set].graphic.subtype = plspec->plot_request[request].graphic.subtype;
      plspec->dataset[set].graphic.thickness = plspec->plot_request[request].graphic.thickness;
      plspec->dataset[set].graphic.fill = plspec->plot_request[request].graphic.fill;

      if (plspec->plot_request[request].split.flags & SPLIT_COLUMNBIN || plspec->plot_request[request].split.flags & SPLIT_PARAMETERCHANGE)
        {
          plspec->plot_request[request].graphic.flags |= GRAPHIC_VARY_EACHPAGE;
          plspec->plot_request[request].graphic.flags |= GRAPHIC_VARY_EACHFILE;
        }

      if (plspec->plot_request[request].graphic.vary &&
          plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_FIXFORNAME)
        {
          if ((request != lastreq) && (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHREQUEST))
            {
              yNames = 0;
            }
          if ((panel != lastpanel) && (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHPAGE))
            {
              yNames = 0;
            }
          if (yNames &&
              (index = match_string(plspec->plot_request[request].yname[plspec->dataset[set].dataname_index],
                                    yName, yNames, EXACT_MATCH)) >= 0)
            {
              memcpy(&plspec->dataset[set].graphic, referenceGraphic[index],
                     sizeof(plspec->dataset[set].graphic));
              continue;
            }
          else
            {
              if (!(yName = SDDS_Realloc(yName, sizeof(*yName) * (yNames + 1))) ||
                  !SDDS_CopyString(&yName[yNames],
                                   plspec->plot_request[request].yname[plspec->dataset[set].dataname_index]) ||
                  !(referenceGraphic = SDDS_Realloc(referenceGraphic, sizeof(*referenceGraphic) * (yNames + 1))))
                SDDS_Bomb("memory allocation failure (determine_graphic_assignments)");
              if (yNames)
                {
                  memcpy(&plspec->dataset[set].graphic, referenceGraphic[yNames - 1],
                         sizeof(plspec->dataset[set].graphic));
                  if (plspec->dataset[set].graphic.flags & GRAPHIC_VARY_TYPE)
                    plspec->dataset[set].graphic.type += 1;
                  if (plspec->dataset[set].graphic.flags & GRAPHIC_VARY_SUBTYPE)
                    plspec->dataset[set].graphic.subtype += 1;
                }
              referenceGraphic[yNames] = &plspec->dataset[set].graphic;
              yNames++;
            }
        }
      if (plspec->plot_request[request].graphic.vary &&
          plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_FIXFORFILE)
        {
          if ((request != lastreq) && (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHREQUEST))
            {
              filenames = 0;
            }
          if ((panel != lastpanel) && (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHPAGE))
            {
              filenames = 0;
            }
          if (filenames &&
              (index = match_string(plspec->plot_request[request].filename[plspec->dataset[set].file_index],
                                    filename, filenames, EXACT_MATCH)) >= 0)
            {
              memcpy(&plspec->dataset[set].graphic, referenceGraphicFN[index],
                     sizeof(plspec->dataset[set].graphic));
              continue;
            }
          else
            {
              if (!(filename = SDDS_Realloc(filename, sizeof(*filename) * (filenames + 1))) ||
                  !SDDS_CopyString(&filename[filenames],
                                   plspec->plot_request[request].filename[plspec->dataset[set].file_index]) ||
                  !(referenceGraphicFN = SDDS_Realloc(referenceGraphicFN, sizeof(*referenceGraphicFN) * (filenames + 1))))
                SDDS_Bomb("memory allocation failure (determine_graphic_assignments)");
              if (filenames)
                {
                  memcpy(&plspec->dataset[set].graphic, referenceGraphicFN[filenames - 1],
                         sizeof(plspec->dataset[set].graphic));
                  if (plspec->dataset[set].graphic.flags & GRAPHIC_VARY_TYPE)
                    plspec->dataset[set].graphic.type += 1;
                  if (plspec->dataset[set].graphic.flags & GRAPHIC_VARY_SUBTYPE)
                    plspec->dataset[set].graphic.subtype += 1;
                }
              referenceGraphicFN[filenames] = &plspec->dataset[set].graphic;
              filenames++;
            }
        }
      else if (panel == lastpanel &&
               plspec->plot_request[request].graphic.vary &&
               ((plspec->dataset[set].datapage == plspec->dataset[set - 1].datapage ||
                 !(plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHPAGE)) &&
                (request == lastreq ||
                 (!(plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHREQUEST) &&
                  plspec->plot_request[lastreq].graphic.vary &&
                  plspec->plot_request[request].graphic.element == plspec->plot_request[lastreq].graphic.element)) &&
                (plspec->dataset[set].file_index == plspec->dataset[set - 1].file_index ||
                 !(plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_EACHFILE))))
        {
          if (request == lastreq && plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_FIXFORREQUEST)
            {
              plspec->dataset[set].graphic.type = plspec->dataset[set - 1].graphic.type;
              plspec->dataset[set].graphic.subtype = plspec->dataset[set - 1].graphic.subtype;
            }
          else
            {
              if (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_TYPE)
                {
                  plspec->dataset[set].graphic.type = plspec->dataset[set - 1].graphic.type + 1;
                }
              if (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_SUBTYPE)
                {
                  plspec->dataset[set].graphic.subtype = plspec->dataset[set - 1].graphic.subtype + 1;
                }
            }
          if (!(plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_TYPE) && !(plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_SUBTYPE))
            bomb("invalid vary mode encountered determining graphic assignments", NULL);
          if (plspec->dataset[set].graphic.element == PLOT_ARROW)
            plspec->dataset[set].graphic.arrow.linetype = plspec->dataset[set - 1].graphic.arrow.linetype + 1;
        }
      plspec->dataset[set].graphic.scale = plspec->plot_request[request].graphic.scale;
      plspec->dataset[set].graphic.connect_linetype = plspec->plot_request[request].graphic.connect_linetype;
      plspec->dataset[set].graphic.fill = plspec->plot_request[request].graphic.fill;
      if (plspec->plot_request[request].graphic.modulus &&
          plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_TYPE)
        plspec->dataset[set].graphic.type =
          (plspec->dataset[set].graphic.type - plspec->plot_request[request].graphic.type) %
          plspec->plot_request[request].graphic.modulus +
          plspec->plot_request[request].graphic.type;
      if (plspec->plot_request[request].graphic.modulus &&
          plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_SUBTYPE)
        plspec->dataset[set].graphic.subtype =
          (plspec->dataset[set].graphic.subtype - plspec->plot_request[request].graphic.subtype) %
          plspec->plot_request[request].graphic.modulus +
          plspec->plot_request[request].graphic.subtype;
      if (plspec->plot_request[request].graphic.flags & GRAPHIC_CONNECT_EQ_TYPE)
        plspec->dataset[set].graphic.connect_linetype = plspec->dataset[set].graphic.type;
      else if (plspec->plot_request[request].graphic.flags & GRAPHIC_CONNECT_EQ_SUBTYPE)
        plspec->dataset[set].graphic.connect_linetype = plspec->dataset[set].graphic.subtype;
      if (plspec->plot_request[request].graphic.flags & GRAPHIC_SUBTYPE_EQ_TYPE)
        plspec->dataset[set].graphic.subtype = plspec->dataset[set].graphic.type;
#if DEBUG
      fprintf(stderr, "data set %ld from req %ld and panel %ld:\n",
              set, request, panel);
      fprintf(stderr, "  request: element=%ld   type=%ld  subtype=%ld  thickness=%ld fill=%ld\n",
              plspec->plot_request[request].graphic.element,
              plspec->plot_request[request].graphic.type,
              plspec->plot_request[request].graphic.subtype,
              plspec->plot_request[request].graphic.thickness,
              plspec->plot_request[request].graphic.fill);
      fprintf(stderr, "  element=%ld  type = %ld  subtype = %ld  thickness=%ld fill=%ld\n",
              plspec->dataset[set].graphic.element,
              plspec->dataset[set].graphic.type,
              plspec->dataset[set].graphic.subtype,
              plspec->dataset[set].graphic.thickness,
              plspec->dataset[set].graphic.fill);
#endif
      lastreq = request;
      lastpanel = panel;
    }
}

void makeEnumeratedScale(long plane,
                         char **enumerate, double *enumPosition, long enumerates,
                         ENUMERATE_SETTINGS *enum_settings,
                         double position1, double position2,
                         double allowedSpace, long adjustForTicks, double tickFraction,
                         long tickLineThickness, long tickLabelThickness,
                         char *labelString, long labelLineType);

void plot_sddsplot_data(PLOT_SPEC *plspec, short initializeDevice)
{
  static long frameEnded = 0;
  double limit0[4] = {0, 0, 0, 0}, *limit = NULL, limit1[4];
  double *pspace, *lspace, *wspace, *x, *y;
  long linetype_default, linethickness_default, idata, panel;
  long ireq, ireqlast, iname, i, j, jj, request, numcolors, numcolors2, makeintensitybar = 0;
  double k;
  long nData, linetype;
  TICK_SETTINGS *tickSettings, *subtickSettings;
  double globalLabelScale;
  PLOT_REQUEST *plreq = NULL;
  char *device_name, **legend;
  GRAPHIC_SPEC **graphic;
  long scalesGroup, plane, otherPlane, index, logMode, tickLinetype, subtickLinetype, timeMode, nSubticks, noLogLabel;
  long tickLinethickness = 0, subtickLinethickness = 0, tickLabelThickness = 0;
  long labelInvert;
  double labelFactor, tickFraction, labelOffset, tickCharScale, tickSpacing;
  double tickModulus, labelPosition, labelHeight, adjustHeightAndPosition, lowerBoundary;
  double subtickFraction;
  long nLegends, maxLegends;
  unsigned long mode, flags, *legendFlags;
  short badScale[2];
  double labelCharScale, labelCharOffset, labelCharThickness;
  long labelCharLineType;
  double titleScale, toplineScale, titleOffset, toplineOffset, titleThickness, toplineThickness;
  long titleLineType, toplineLineType;
  long typeIncr = 1000, dataOrTickStep;
  double min_level, max_level;
  long coloroffset = 0;
  long ncolor_min = 65535;
  long ncolor_max = 0;

  graphic = NULL;
  legend = NULL;
  legendFlags = NULL;
  nLegends = maxLegends = 0;
  min_level = DBL_MAX;
  max_level = -DBL_MAX;
  device_name = plspec->device ? plspec->device : getenv("SDDS_DEVICE");

  if (initializeDevice)
    {
      if (change_term(device_name, strlen(device_name)) == -1)
        {
          fprintf(stderr, "error: unknown graphics device.\n");
          list_terms(stderr);
          return;
        }
    }
  else if (!frameEnded)
    {
      frame_end(1);
      frameEnded = 1;
    }

  for (request = 0; request < plspec->plot_requests; request++)
    {
      /*
        min_level=DBL_MAX;
        max_level=-DBL_MAX;
      */
      j = 0;
      for (panel = 0; panel < plspec->panels; panel++)
        {
          jj = 0;
          for (idata = 0; idata < plspec->panel[panel].datasets; idata++)
            {
              /*
                if (plspec->panel[panel].dataset[idata]->split_min<min_level)
                min_level=plspec->panel[panel].dataset[idata]->split_min;
                if (plspec->panel[panel].dataset[idata]->split_max>max_level)
                max_level=plspec->panel[panel].dataset[idata]->split_max;
              */
              if (plspec->panel[panel].dataset[idata]->request_index == request)
                {
                  jj++;
                  if (plspec->panel[panel].dataset[idata]->graphic.flags & GRAPHIC_VARY_SUBTYPE)
                    {
                      if (plspec->panel[panel].dataset[idata]->graphic.subtype > ncolor_max)
                        ncolor_max = plspec->panel[panel].dataset[idata]->graphic.subtype;
                      if (plspec->panel[panel].dataset[idata]->graphic.subtype < ncolor_min)
                        ncolor_min = plspec->panel[panel].dataset[idata]->graphic.subtype;
                    }
                  if (plspec->panel[panel].dataset[idata]->graphic.flags & GRAPHIC_VARY_TYPE)
                    {
                      if (plspec->panel[panel].dataset[idata]->graphic.type > ncolor_max)
                        ncolor_max = plspec->panel[panel].dataset[idata]->graphic.type;
                      if (plspec->panel[panel].dataset[idata]->graphic.type < ncolor_min)
                        ncolor_min = plspec->panel[panel].dataset[idata]->graphic.type;
                    }
                }
            }
          if (jj > j)
            j = jj;
        }
      plreq = plspec->plot_request + request;
      plreq->color_settings.num = j;
      if (plreq->color_settings.flags & COLORSET_SPECTRAL)
        {
          if (j != (ncolor_max + 1 - ncolor_min))
            j = ncolor_max + 1 - ncolor_min;
          if (j < 1)
            j = 1;
          if (j == 1)
            {
              plreq->color_settings.increment[0] = 0;
            }
          else
            {
              plreq->color_settings.increment[0] = 393216.0 / (j - 1);
            }
          k = 0;
          (term_tbl[term].add_color)(0, 0, 0, 1);
          for (i = 0; i < j; i++)
            {
              if (k < 65536)
                {
                  //red to orange
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)((k / (double)(65535)) * (256 * 165)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 65536) && (k < 131072))
                {
                  //orange to yellow
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)(((k - 65536) / (double)(131071 - 65536)) * (256 * 91) + (256 * 165 - 1)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 131072) && (k < 150733))
                {
                  //yellow to green (heavy yellow)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 131072) / (double)(150732 - 131072))) * (256 * 51) + (256 * 205 - 1)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 150732) && (k < 196608))
                {
                  //yellow to green (heavy green)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 150732) / (double)(196607 - 150732))) * (256 * 205)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 196608) && (k < 235930))
                {
                  //green to cyan (heavy green)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 196608) / (double)(235929 - 196608)) * (256 * 215)),
                                             0);
                }
              else if ((k >= 235930) && (k < 262144))
                {
                  //green to cyan (heavy cyan)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 235930) / (double)(262143 - 235930)) * (256 * 41) + (256 * 215 - 1)),
                                             0);
                }
              else if ((k >= 262144) && (k < 327680))
                {
                  //cyan to blue
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)((1 - ((k - 262144) / (double)(327679 - 262144))) * (256 * 255)),
                                             (unsigned short)(65535),
                                             0);
                }
              else
                {
                  //blue to violet
                  (term_tbl[term].add_color)((unsigned short)(((k - 327680) / (double)(393215 - 327680)) * (256 * 255)),
                                             (unsigned short)(0),
                                             (unsigned short)(65535),
                                             0);
                }
              k = (i + 1) * plreq->color_settings.increment[0];
            }
          plreq->color_settings.flags |= COLORSET_USERDEFINED;
        }
      else if (plreq->color_settings.flags & COLORSET_RSPECTRAL)
        {
          if (j != (ncolor_max + 1 - ncolor_min))
            j = ncolor_max + 1 - ncolor_min;
          if (j < 1)
            j = 1;
          if (j == 1)
            {
              plreq->color_settings.increment[0] = 0;
            }
          else
            {
              plreq->color_settings.increment[0] = 393216.0 / (j - 1);
            }
          k = 393216;
          (term_tbl[term].add_color)(0, 0, 0, 1);
          for (i = j - 1; i >= 0; i--)
            {
              if (k < 65536)
                {
                  //orange to red
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)((k / (double)(65535)) * (256 * 165)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 65536) && (k < 131072))
                {
                  //yellow to orange
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)(((k - 65536) / (double)(131071 - 65536)) * (256 * 91) + (256 * 165 - 1)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 131072) && (k < 150733))
                {
                  //green to yellow (heavy yellow)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 131072) / (double)(150732 - 131072))) * (256 * 51) + (256 * 205 - 1)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 150732) && (k < 196608))
                {
                  //green to yellow (heavy green)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 150732) / (double)(196607 - 150732))) * (256 * 205)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 196608) && (k < 235930))
                {
                  //cyan to green (heavy green)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 196608) / (double)(235929 - 196608)) * (256 * 215)),
                                             0);
                }
              else if ((k >= 235930) && (k < 262144))
                {
                  //cyan to green (heavy cyan)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 235930) / (double)(262143 - 235930)) * (256 * 41) + (256 * 215 - 1)),
                                             0);
                }
              else if ((k >= 262144) && (k < 327680))
                {
                  //blue to cyan
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)((1 - ((k - 262144) / (double)(327679 - 262144))) * (256 * 255)),
                                             (unsigned short)(65535),
                                             0);
                }
              else
                {
                  //violet to blue
                  (term_tbl[term].add_color)((unsigned short)(((k - 327680) / (double)(393215 - 327680)) * (256 * 255)),
                                             (unsigned short)(0),
                                             (unsigned short)(65535),
                                             0);
                }
              k = (i - 1) * plreq->color_settings.increment[0];
            }
          plreq->color_settings.flags |= COLORSET_USERDEFINED;
        }
      else if (plreq->color_settings.flags & COLORSET_RTEMPERATURE)
        {
          if (j != (ncolor_max + 1 - ncolor_min))
            j = ncolor_max + 1 - ncolor_min;
          if (j < 1)
            j = 1;
          if (j == 1)
            {
              plreq->color_settings.increment[0] = 0;
            }
          else
            {
              plreq->color_settings.increment[0] = 327680.0 / (j - 1);
            }
          k = 0; //start with red
          (term_tbl[term].add_color)(0, 0, 0, 1);
          for (i = 0; i < j; i++)
            {
              if (k < 65536)
                {
                  //red to orange
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)((k / (double)(65535)) * (256 * 165)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 65536) && (k < 131072))
                {
                  //orange to yellow
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)(((k - 65536) / (double)(131071 - 65536)) * (256 * 91) + (256 * 165 - 1)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 131072) && (k < 150733))
                {
                  //yellow to green (heavy yellow)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 131072) / (double)(150732 - 131072))) * (256 * 51) + (256 * 205 - 1)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 150732) && (k < 196608))
                {
                  //yellow to green (heavy green)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 150732) / (double)(196607 - 150732))) * (256 * 205)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 196608) && (k < 235930))
                {
                  //green to cyan (heavy green)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 196608) / (double)(235929 - 196608)) * (256 * 215)),
                                             0);
                }
              else if ((k >= 235930) && (k < 262144))
                {
                  //green to cyan (heavy cyan)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 235930) / (double)(262143 - 235930)) * (256 * 41) + (256 * 215 - 1)),
                                             0);
                }
              else
                {
                  //cyan to blue
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(k == 262144 ? 65535 : 327680 - k),
                                             (unsigned short)(65535),
                                             0);
                }
              k = (i + 1) * plreq->color_settings.increment[0];
            }
          plreq->color_settings.flags |= COLORSET_USERDEFINED;
        }
      else if (plreq->color_settings.flags & COLORSET_TEMPERATURE)
        {
          if (j != (ncolor_max + 1 - ncolor_min))
            j = ncolor_max + 1 - ncolor_min;
          if (j < 1)
            j = 1;
          if (j == 1)
            {
              plreq->color_settings.increment[0] = 0;
            }
          else
            {
              plreq->color_settings.increment[0] = 327680.0 / (j - 1);
            }
          k = 327680; //start with blue
          (term_tbl[term].add_color)(0, 0, 0, 1);
          for (i = j - 1; i >= 0; i--)
            {
              if (k < 65536)
                {
                  //red to orange
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)((k / (double)(65535)) * (256 * 165)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 65536) && (k < 131072))
                {
                  //orange to yellow
                  (term_tbl[term].add_color)((unsigned short)(65535),
                                             (unsigned short)(((k - 65536) / (double)(131071 - 65536)) * (256 * 91) + (256 * 165 - 1)),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 131072) && (k < 150733))
                {
                  //yellow to green (heavy yellow)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 131072) / (double)(150732 - 131072))) * (256 * 51) + (256 * 205 - 1)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 150732) && (k < 196608))
                {
                  //yellow to green (heavy green)
                  (term_tbl[term].add_color)((unsigned short)((1 - ((k - 150732) / (double)(196607 - 150732))) * (256 * 205)),
                                             (unsigned short)(65535),
                                             (unsigned short)(0),
                                             0);
                }
              else if ((k >= 196608) && (k < 235930))
                {
                  //green to cyan (heavy green)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 196608) / (double)(235929 - 196608)) * (256 * 215)),
                                             0);
                }
              else if ((k >= 235930) && (k < 262144))
                {
                  //green to cyan (heavy cyan)
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(65535),
                                             (unsigned short)(((k - 235930) / (double)(262143 - 235930)) * (256 * 41) + (256 * 215 - 1)),
                                             0);
                }
              else
                {
                  //cyan to blue
                  (term_tbl[term].add_color)((unsigned short)(0),
                                             (unsigned short)(k == 262144 ? 65535 : 327680 - k),
                                             (unsigned short)(65535),
                                             0);
                }
              k = (i - 1) * plreq->color_settings.increment[0];
            }
          plreq->color_settings.flags |= COLORSET_USERDEFINED;
        }
      else
        {
          if ((plreq->color_settings.flags & COLORSET_START) && (plreq->color_settings.flags & COLORSET_FINISH))
            {
              if (j == 1)
                {
                  plreq->color_settings.increment[0] = 0;
                  plreq->color_settings.increment[1] = 0;
                  plreq->color_settings.increment[2] = 0;
                }
              else
                {
                  plreq->color_settings.increment[0] = (plreq->color_settings.red[1] - plreq->color_settings.red[0]) * 1.0 / (j - 1);
                  plreq->color_settings.increment[1] = (plreq->color_settings.green[1] - plreq->color_settings.green[0]) * 1.0 / (j - 1);
                  plreq->color_settings.increment[2] = (plreq->color_settings.blue[1] - plreq->color_settings.blue[0]) * 1.0 / (j - 1);
                }
              plreq->color_settings.flags |= COLORSET_INCREMENT;
            }
          if ((plreq->color_settings.flags & COLORSET_START) && (plreq->color_settings.flags & COLORSET_INCREMENT))
            {
              if (!(plreq->color_settings.flags & COLORSET_FINISH))
                {
                  plreq->color_settings.red[1] = (unsigned short)(plreq->color_settings.red[0] + (j - 1) * plreq->color_settings.increment[0]);
                  plreq->color_settings.green[1] = (unsigned short)(plreq->color_settings.green[0] + (j - 1) * plreq->color_settings.increment[1]);
                  plreq->color_settings.blue[1] = (unsigned short)(plreq->color_settings.blue[0] + (j - 1) * plreq->color_settings.increment[2]);
                }
              (term_tbl[term].add_color)(0, 0, 0, 1);
              for (i = 0; i < j; i++)
                {
                  (term_tbl[term].add_color)((unsigned short)(plreq->color_settings.red[0] + i * plreq->color_settings.increment[0]),
                                             (unsigned short)(plreq->color_settings.green[0] + i * plreq->color_settings.increment[1]),
                                             (unsigned short)(plreq->color_settings.blue[0] + i * plreq->color_settings.increment[2]),
                                             0);
                }
              plreq->color_settings.flags |= COLORSET_USERDEFINED;
            }
        }
      if (plreq->color_settings.flags & COLORSET_USERDEFINED)
        {
          for (panel = 0; panel < plspec->panels; panel++)
            {
              int offsettype = -1;
              for (idata = 0; idata < plspec->panel[panel].datasets; idata++)
                {
                  if (plspec->panel[panel].dataset[idata]->request_index == request)
                    {
                      if (plspec->plot_request[request].graphic.flags & GRAPHIC_VARY_SUBTYPE)
                        {
                          if (offsettype == -1)
                            {
                              offsettype = plspec->panel[panel].dataset[idata]->graphic.subtype;
                            }
                          plspec->panel[panel].dataset[idata]->graphic.subtype += typeIncr;
                          plspec->panel[panel].dataset[idata]->graphic.subtype -= offsettype;
                          if (plspec->plot_request[request].graphic.flags & GRAPHIC_CONNECT_EQ_SUBTYPE)
                            {
                              plspec->panel[panel].dataset[idata]->graphic.connect_linetype += typeIncr;
                              plspec->panel[panel].dataset[idata]->graphic.connect_linetype -= offsettype;
                            }
                        }
                      else
                        {
                          if (offsettype == -1)
                            {
                              offsettype = plspec->panel[panel].dataset[idata]->graphic.type;
                            }
                          plspec->panel[panel].dataset[idata]->graphic.type += typeIncr;
                          plspec->panel[panel].dataset[idata]->graphic.type -= offsettype;
                          if (plspec->plot_request[request].graphic.flags & GRAPHIC_CONNECT_EQ_TYPE)
                            {
                              plspec->panel[panel].dataset[idata]->graphic.connect_linetype += typeIncr;
                              plspec->panel[panel].dataset[idata]->graphic.connect_linetype -= offsettype;
                            }
                        }
                    }
                }
            }
          typeIncr += j;
        }
    }

  linetype_default = 0;
  linethickness_default = 0;
  set_title_at_top(1);
  for (panel = 0; panel < plspec->panels; panel++)
    {
      min_level = DBL_MAX;
      max_level = -DBL_MAX;
      nData = 0;
      linetype_default = plspec->panel[panel].linetypeDefault;
      linethickness_default = plspec->panel[panel].linethicknessDefault;
      if (plspec->panel[panel].ix == 0 && plspec->panel[panel].iy == 0)
        graphics_on();
      flags = plspec->panel[panel].flags;
      pspace = plspec->panel[panel].pspace;
      wspace = plspec->panel[panel].workSpace;
      set_pspace(pspace[0], pspace[1], pspace[2], pspace[3]);
      set_wspace(wspace[0], wspace[1], wspace[2], wspace[3]);
      titleScale = toplineScale = 1;
      titleOffset = toplineOffset = 0;
      titleLineType = toplineLineType = 0;
      titleThickness = toplineThickness = 0;
      if ((globalLabelScale = plspec->panel[panel].labelSize / 0.03) <= 0)
        globalLabelScale = 1;

      for (dataOrTickStep = 0; dataOrTickStep <= 1; dataOrTickStep++)
        {
          for (idata = 0; idata < plspec->panel[panel].datasets; idata++)
            {
              plreq = plspec->plot_request + plspec->panel[panel].dataset[idata]->request_index;
              if (plspec->panel[panel].dataset[idata]->split_min < min_level)
                min_level = plspec->panel[panel].dataset[idata]->split_min;
              if (plspec->panel[panel].dataset[idata]->split_max > max_level)
                max_level = plspec->panel[panel].dataset[idata]->split_max;
              if (plspec->panel[panel].titleSpec[0].flags & LABEL_SCALE_GIVEN)
                titleScale = plspec->panel[panel].titleSpec[0].scale;
              if (plspec->panel[panel].titleSpec[1].flags & LABEL_SCALE_GIVEN)
                toplineScale = plspec->panel[panel].titleSpec[1].scale;
              if (plspec->panel[panel].titleSpec[0].flags & LABEL_OFFSET_GIVEN)
                titleOffset = plspec->panel[panel].titleSpec[0].offset;
              if (plspec->panel[panel].titleSpec[1].flags & LABEL_OFFSET_GIVEN)
                toplineOffset = plspec->panel[panel].titleSpec[1].offset;
              if (plspec->panel[panel].titleSpec[0].flags & LABEL_THICKNESS_GIVEN)
                titleThickness = plspec->panel[panel].titleSpec[0].thickness;
              if (plspec->panel[panel].titleSpec[1].flags & LABEL_THICKNESS_GIVEN)
                {
                  toplineThickness = plspec->panel[panel].titleSpec[1].thickness;
                }
              else
                {
                  /* This is needed to set the thickness for the -filenameOnTopline option */
                  toplineThickness = plreq->label[3].thickness;
                }
              if (plspec->panel[panel].titleSpec[0].flags & LABEL_LINETYPE_GIVEN)
                titleLineType = plspec->panel[panel].titleSpec[0].linetype;
              if (plspec->panel[panel].titleSpec[1].flags & LABEL_LINETYPE_GIVEN)
                toplineLineType = plspec->panel[panel].titleSpec[1].linetype;

              limit = limit0;
              for (plane = 0; plane < 2; plane++)
                {
                  scalesGroup = plspec->panel[panel].dataset[idata]->scalesGroupIndex[plane];
                  limit[0 + 2 * plane] = plspec->scalesGroupData[plane][scalesGroup].limit[0];
                  limit[1 + 2 * plane] = plspec->scalesGroupData[plane][scalesGroup].limit[1];
                  if (limit[1 + 2 * plane] - limit[0 + 2 * plane] < 2e-305) {
                    limit[1 + 2 * plane] = (plspec->scalesGroupData[plane][scalesGroup].limit[0] + plspec->scalesGroupData[plane][scalesGroup].limit[1]) / 2 + 1e-305;
                    limit[0 + 2 * plane] = (plspec->scalesGroupData[plane][scalesGroup].limit[0] + plspec->scalesGroupData[plane][scalesGroup].limit[1]) / 2 - 1e-305;
                  }
                }

              badScale[0] = badScale[1] = 0;
              if (limit[0] >= limit[1] || limit[2] >= limit[3])
                {
                  if (limit[0] >= limit[1])
                    {
                      limit[0] = 0;
                      limit[1] = 1;
                      badScale[0] = 1;
                    }
                  if (limit[2] >= limit[3])
                    {
                      limit[2] = 0;
                      limit[3] = 1;
                      badScale[1] = 1;
                    }
                }
              else
                {
                  if (plreq->aspect_ratio)
                    {
                      computeAspectAdjustedLimits(limit1, limit, plreq->aspect_ratio);
                      limit = limit1;
                    }
                }
              set_mapping(limit[0], limit[1], limit[2], limit[3]);
              set_default_char_size();

              if ((dataOrTickStep == 1 && dataBehind) || (dataOrTickStep == 0 && !dataBehind))
                {
                  for (plane = 0; plane < 2; plane++)
                    {
                      scalesGroup = plspec->panel[panel].dataset[idata]->scalesGroupIndex[plane];
                      if (!(plspec->scaleLabelInfo[plane][scalesGroup].flags & SCALE_LABEL_DONE) &&
                          plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber != -1)
                        {
                          mode = plspec->scalesGroupData[plane][scalesGroup].mode;
                          tickSettings = subtickSettings = NULL;
                          if ((index = plspec->scalesGroupData[plane][scalesGroup].tickSettingsRequestIndex) != -1)
                            tickSettings = &plspec->plot_request[index].tick_settings;
                          if ((index = plspec->scalesGroupData[plane][scalesGroup].subtickSettingsRequestIndex) != -1)
                            subtickSettings = &plspec->plot_request[index].subtick_settings;
                          otherPlane = plane ? 0 : 1;
                          tickLinetype = tickLinethickness = logMode = timeMode = labelInvert = 0;
                          labelFactor = tickCharScale = 1;
                          tickFraction = -1;
                          labelOffset = tickModulus = tickSpacing = 0;
                          if (plane == 0)
                            {
                              if (((mode & (MODE_X_SPECIALSCALES)) && (mode & (MODE_X_LOG))) ||
                                  (tickSettings && tickSettings->flags & (TICKSET_XLOGARITHMIC)))
                                {
                                  if (tickSettings)
                                    logMode = tickSettings->flags & (TICKSET_XNONEXPLABELS) ? 2 : 1;
                                  else
                                    logMode = 2;
                                }
                              if (subtickSettings && subtickSettings->flags & (TICKSET_XNOLOGLABEL))
                                noLogLabel = 1;
                              else
                                noLogLabel = 0;
                              tickLabelThickness = tickSettings->labelThickness[plane];
                              if (tickSettings && tickSettings->flags & (TICKSET_XOPTS))
                                {
                                  if (tickSettings->flags & (TICKSET_XGRID))
                                    tickFraction = 1;
                                  if (tickSettings->flags & (TICKSET_XLINETYPE))
                                    tickLinetype = tickSettings->linetype[plane];
                                  if (tickSettings->flags & (TICKSET_XTHICKNESS))
                                    tickLinethickness = tickSettings->thickness[plane];
                                  if (tickSettings->flags & (TICKSET_XSPACING))
                                    tickSpacing = tickSettings->spacing[plane];
                                  if (tickSettings->flags & (TICKSET_XFRACTION))
                                    tickFraction = tickSettings->fraction[plane];
                                  if (tickSettings->flags & (TICKSET_XMODULUS))
                                    tickModulus = tickSettings->modulus[plane];
                                  if (tickSettings->flags & (TICKSET_XFACTOR))
                                    labelFactor = tickSettings->factor[plane];
                                  if (tickSettings->flags & (TICKSET_XOFFSET))
                                    labelOffset = tickSettings->offset[plane];
                                  if (tickSettings->flags & (TICKSET_XTIME))
                                    timeMode = 1;
                                  if (tickSettings->flags & (TICKSET_XINVERT))
                                    labelInvert = 1;
                                  if (tickSettings->flags & (TICKSET_XSCALECHAR))
                                    tickCharScale = tickSettings->scaleChar[plane];
                                }
                              tickCharScale *= globalLabelScale;

                              nSubticks = subtickFraction = 0;
                              subtickLinetype = tickLinetype;
                              subtickLinethickness = tickLinethickness;
                              if (subtickSettings)
                                {
                                  if (subtickSettings->flags & (TICKSET_XDIVISIONS))
                                    nSubticks = subtickSettings->divisions[plane];
                                  if (subtickSettings->flags & (TICKSET_XLINETYPE))
                                    subtickLinetype = subtickSettings->linetype[plane];
                                  if (subtickSettings->flags & (TICKSET_XTHICKNESS))
                                    subtickLinethickness = subtickSettings->thickness[plane];
                                  if (subtickSettings->flags & (TICKSET_XGRID))
                                    subtickFraction = 1;
                                  else if (subtickSettings->flags & (TICKSET_XFRACTION))
                                    subtickFraction = subtickSettings->fraction[plane];
                                }
                            }
                          else
                            {
                              if (((mode & (MODE_Y_SPECIALSCALES)) && (mode & (MODE_Y_LOG))) ||
                                  (tickSettings && tickSettings->flags & (TICKSET_YLOGARITHMIC)))
                                {
                                  if (tickSettings)
                                    logMode = tickSettings->flags & (TICKSET_YNONEXPLABELS) ? 2 : 1;
                                  else
                                    logMode = 2;
                                }
                              if (subtickSettings && subtickSettings->flags & (TICKSET_YNOLOGLABEL))
                                noLogLabel = 1;
                              else
                                noLogLabel = 0;
                              tickLabelThickness = tickSettings->labelThickness[plane];
                              if (tickSettings && tickSettings->flags & (TICKSET_YOPTS))
                                {
                                  if (tickSettings->flags & (TICKSET_YGRID))
                                    tickFraction = 1;
                                  if (tickSettings->flags & (TICKSET_YLINETYPE))
                                    tickLinetype = tickSettings->linetype[plane];
                                  if (tickSettings->flags & (TICKSET_YTHICKNESS))
                                    tickLinethickness = tickSettings->thickness[plane];
                                  if (tickSettings->flags & (TICKSET_YSPACING))
                                    tickSpacing = tickSettings->spacing[plane];
                                  if (tickSettings->flags & (TICKSET_YFRACTION))
                                    tickFraction = tickSettings->fraction[plane];
                                  if (tickSettings->flags & (TICKSET_YMODULUS))
                                    tickModulus = tickSettings->modulus[plane];
                                  if (tickSettings->flags & (TICKSET_YFACTOR))
                                    labelFactor = tickSettings->factor[plane];
                                  if (tickSettings->flags & (TICKSET_YOFFSET))
                                    labelOffset = tickSettings->offset[plane];
                                  if (tickSettings->flags & (TICKSET_YTIME))
                                    timeMode = 1;
                                  if (tickSettings->flags & (TICKSET_YINVERT))
                                    labelInvert = 1;
                                  if (tickSettings->flags & (TICKSET_YSCALECHAR))
                                    tickCharScale = tickSettings->scaleChar[plane];
                                }
                              tickCharScale *= globalLabelScale;

                              nSubticks = subtickFraction = 0;
                              subtickLinetype = tickLinetype;
                              subtickLinethickness = tickLinethickness;
                              if (subtickSettings)
                                {
                                  if (subtickSettings->flags & (TICKSET_YDIVISIONS))
                                    nSubticks = subtickSettings->divisions[plane];
                                  if (subtickSettings->flags & (TICKSET_YLINETYPE))
                                    subtickLinetype = subtickSettings->linetype[plane];
                                  if (subtickSettings->flags & (TICKSET_YTHICKNESS))
                                    subtickLinethickness = subtickSettings->thickness[plane];
                                  if (subtickSettings->flags & (TICKSET_YGRID))
                                    subtickFraction = 1;
                                  else if (subtickSettings->flags & (TICKSET_YFRACTION))
                                    subtickFraction = subtickSettings->fraction[plane];
                                }
                            }
                          if (!(flags & PLREQ_TOPTITLE) && plane == 0)
                            lowerBoundary = wspace[2 * otherPlane] + 1. / 3. * (pspace[2 * otherPlane] - wspace[2 * otherPlane]);
                          else
                            lowerBoundary = wspace[2 * otherPlane];
                          if ((plreq->scalesGroupSpec[plane].flags & SCALESGROUP_OTHER_SIDE) == 0)
                            labelPosition = limit[0 + 2 * otherPlane] -
                              (limit[1 + 2 * otherPlane] - limit[0 + 2 * otherPlane]) / (pspace[1 + 2 * otherPlane] - pspace[0 + 2 * otherPlane]) *
                              (plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber - 1.0) /
                              plspec->panel[panel].scalesUsed[plane] *
                              (pspace[0 + 2 * otherPlane] - lowerBoundary);
                          else
                            labelPosition = limit[1 + 2 * otherPlane];
                          labelHeight = (pspace[2 * otherPlane] - lowerBoundary) / plspec->panel[panel].scalesUsed[plane] *
                            (limit[1 + 2 * otherPlane] - limit[0 + 2 * otherPlane]) / (pspace[1 + 2 * otherPlane] - pspace[0 + 2 * otherPlane]);
                          adjustHeightAndPosition = 0;
                          if (plspec->panel[panel].scalesUsed[plane] != 1 &&
                              plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber != 1)
                            {
                              adjustHeightAndPosition = 1;
                            }
                          /* determine if the user has specified scaling or offseting of the x or y scale label
                           */
                          labelCharScale = 1;
                          labelCharOffset = 0;
                          labelCharThickness = 0;
                          labelCharLineType = 0;
                          if (plspec->scaleLabelInfo[plane][scalesGroup].flags & SCALE_LABEL_LINETYPEGIVEN)
                            labelCharLineType = plspec->scaleLabelInfo[plane][scalesGroup].linetype;
                          if (plspec->scaleLabelInfo[plane][scalesGroup].flags & SCALE_LABEL_SCALEGIVEN)
                            labelCharScale = plspec->scaleLabelInfo[plane][scalesGroup].scale;
                          if (plspec->scaleLabelInfo[plane][scalesGroup].flags & SCALE_LABEL_OFFSETGIVEN)
                            labelCharOffset = plspec->scaleLabelInfo[plane][scalesGroup].offset;
                          if (plspec->scaleLabelInfo[plane][scalesGroup].flags & SCALE_LABEL_THICKNESSGIVEN)
                            labelCharThickness = plspec->scaleLabelInfo[plane][scalesGroup].thickness;
                          labelCharScale *= globalLabelScale;

                          /* make the scales and label */
                          if (plane && plspec->panel[panel].flags & PLREQ_VPRINTDOWN)
                            {
                              set_vertical_print_direction(-1);
                            }
                          if (badScale[plane] || flags & (PLREQ_NOSCALESX << plane))
                            {
                              if (!(flags & PLREQ_NOBORDER))
                                {
                                  make_scale(plane, logMode,
                                             0, /* do tick labels ? */
                                             0.0, 10, tickLinetype, tickLinethickness, tickLabelThickness,
                                             labelFactor, 0, labelInvert, labelOffset, tickModulus, tickCharScale,
                                             0, subtickFraction, subtickLinetype, subtickLinethickness,
                                             plreq->scalesGroupSpec[plane].flags & SCALESGROUP_OTHER_SIDE ? 1 : 0, /* side (location) */
                                             limit[0 + 2 * plane],                                                 /* starting value */
                                             limit[1 + 2 * plane],                                                 /* ending value */
                                             labelPosition,
                                             labelHeight,
                                             adjustHeightAndPosition,
                                             plspec->scaleLabelInfo[plane][scalesGroup].label ? plspec->scaleLabelInfo[plane][scalesGroup].label : NULL, /* label string */
                                             labelCharScale, labelCharOffset, labelCharLineType,
                                             labelCharThickness, noLogLabel);
                                  if (plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber == 1)
                                    {
                                      make_scale(plane, logMode,
                                                 0, /* do tick labels ? */
                                                 0.0, 10, tickLinetype, tickLinethickness, tickLabelThickness,
                                                 labelFactor, 0, labelInvert, labelOffset, tickModulus, 1,
                                                 0, subtickFraction, subtickLinetype, subtickLinethickness,
                                                 plreq->scalesGroupSpec[plane].flags & SCALESGROUP_OTHER_SIDE ? 0 : 1, /* side (location) */
                                                 limit[0 + 2 * plane],                                                 /* starting value */
                                                 limit[1 + 2 * plane],                                                 /* ending value */
                                                 limit[1 + 2 * otherPlane],                                            /* position */
                                                 labelHeight,
                                                 0,
                                                 NULL, 1, 0, 0, 0, noLogLabel);
                                    }
                                }
                            }
                          else if (plspec->panel[panel].dataset[idata]->enumerate &&
                                   plane == plspec->panel[panel].dataset[idata]->enumPlane)
                            {
                              makeEnumeratedScale(plane,
                                                  plspec->panel[panel].dataset[idata]->enumerate,
                                                  plane ? plspec->panel[panel].dataset[idata]->y : plspec->panel[panel].dataset[idata]->x,
                                                  plspec->panel[panel].dataset[idata]->points,
                                                  &plreq->enumerate_settings, labelPosition,
                                                  (plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber == 1 ? limit[1 + 2 * otherPlane] : labelPosition),
                                                  plane == 0 && plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT ? 0 : labelHeight, 
                                                  adjustHeightAndPosition, tickFraction,
                                                  tickLinethickness, tickLabelThickness,
                                                  plspec->scaleLabelInfo[plane][scalesGroup].label ? plspec->scaleLabelInfo[plane][scalesGroup].label : NULL /* label string */,
                                                  labelCharLineType);
                            }
                          else if (timeMode)
                            {
                              makeTimeScales(plane, 0.0, tickLinetype,
                                             labelPosition,
                                             (plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber == 1 ? limit[1 + 2 * otherPlane] : labelPosition),
                                             plane == 0 && plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT ? "" : (plspec->scaleLabelInfo[plane][scalesGroup].label ? plspec->scaleLabelInfo[plane][scalesGroup].label : NULL), /* label string */
                                             labelCharLineType, labelHeight, adjustHeightAndPosition, labelCharScale,
                                             plane == 0 && plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT ? 0 : tickCharScale,
                                             tickLinethickness, tickLabelThickness, labelCharThickness);
                            }
                          else
                            {
                              make_scale(plane, logMode,
                                         plane == 0 && plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT ? 0 : 1, /* do tick labels ? */
                                         tickFraction, tickSpacing, tickLinetype, tickLinethickness, tickLabelThickness,
                                         labelFactor, 0, labelInvert, labelOffset, tickModulus, tickCharScale,
                                         nSubticks, subtickFraction, subtickLinetype, subtickLinethickness,
                                         plreq->scalesGroupSpec[plane].flags & SCALESGROUP_OTHER_SIDE ? 1 : 0, /* side (location) */
                                         limit[0 + 2 * plane],                                                 /* starting value */
                                         limit[1 + 2 * plane],                                                 /* ending value */
                                         labelPosition,
                                         labelHeight,
                                         adjustHeightAndPosition,
                                         plspec->scaleLabelInfo[plane][scalesGroup].label && !(plane == 0 && plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT) ? plspec->scaleLabelInfo[plane][scalesGroup].label : NULL, /* label string */
                                         labelCharScale, labelCharOffset, labelCharLineType, labelCharThickness, noLogLabel);
                              if (plspec->scaleLabelInfo[plane][scalesGroup].scaleNumber == 1)
                                make_scale(plane, logMode,
                                           0, /* do tick labels ? */
                                           tickFraction, tickSpacing, tickLinetype, tickLinethickness, tickLabelThickness,
                                           labelFactor, 0, labelInvert, labelOffset, tickModulus, 1,
                                           nSubticks, subtickFraction, subtickLinetype, subtickLinethickness,
                                           plreq->scalesGroupSpec[plane].flags & SCALESGROUP_OTHER_SIDE ? 0 : 1, /* side (location) */
                                           limit[0 + 2 * plane],                                                 /* starting value */
                                           limit[1 + 2 * plane],                                                 /* ending value */
                                           limit[1 + 2 * otherPlane],                                            /* position */
                                           labelHeight,
                                           0,
                                           NULL, 1, 0, 0, 0, noLogLabel);
                            }
                          if (plane && plspec->panel[panel].flags & PLREQ_VPRINTDOWN)
                            {
                              set_vertical_print_direction(1);
                            }
                          plspec->scaleLabelInfo[plane][scalesGroup].flags |= SCALE_LABEL_DONE;
                        }
                    } /* loop over planes for labels and scales */
                }

              if ((dataOrTickStep == 0 && dataBehind) || (dataOrTickStep == 1 && !dataBehind))
                {
                  if (!plspec->panel[panel].dataset[idata]->points)
                    continue;

                  nData += plspec->panel[panel].dataset[idata]->points;

                  if (plspec->panel[panel].flags & PLREQ_AXESX && limit[2] < 0 && limit[3] > 0)
                    {
                      linetype = set_linetype(plspec->panel[panel].axesLinetype);
                      set_linethickness(plspec->panel[panel].axesLinethickness);
                      pmove(limit[0], 0.0);
                      pdraw(limit[1], 0.0, 1);
                      set_linetype(linetype);
                      set_linethickness(0);
                    }
                  if (plspec->panel[panel].flags & PLREQ_AXESY && limit[0] < 0 && limit[1] > 0)
                    {
                      linetype = set_linetype(plspec->panel[panel].axesLinetype);
                      set_linethickness(plspec->panel[panel].axesLinethickness);
                      pmove(0.0, limit[2]);
                      pdraw(0.0, limit[3], 1);
                      set_linetype(linetype);
                      set_linethickness(0);
                    }

                  if (plreq->overlay.flags)
                    {
                      if (!(x = SDDS_Malloc(sizeof(*x) * plspec->panel[panel].dataset[idata]->points)) ||
                          !(y = SDDS_Malloc(sizeof(*y) * plspec->panel[panel].dataset[idata]->points)) ||
                          !adjust_sddsplot_overlay_data(x, plspec->panel[panel].dataset[idata]->x,
                                                        plspec->panel[panel].dataset[idata]->points,
                                                        limit[1] - limit[0], limit[0], &plreq->overlay, 0) ||
                          !adjust_sddsplot_overlay_data(y, plspec->panel[panel].dataset[idata]->y,
                                                        plspec->panel[panel].dataset[idata]->points,
                                                        limit[3] - limit[2], limit[2], &plreq->overlay, 1))
                        continue;
                    }
                  else
                    {
                      x = plspec->panel[panel].dataset[idata]->x;
                      y = plspec->panel[panel].dataset[idata]->y;
                    }
                  sddsplotDataArrays(x, y,
                                     plspec->panel[panel].dataset[idata]->x1,
                                     plspec->panel[panel].dataset[idata]->y1,
                                     plspec->panel[panel].dataset[idata]->points,
                                     plreq->flags & PLREQ_XGAP ? plreq->xgap : 0.0,
                                     plreq->flags & PLREQ_YGAP ? plreq->ygap : 0.0,
                                     &plspec->panel[panel].dataset[idata]->graphic,
                                     plreq->flags, plspec->panel[panel].pspace);
                  labelDataPoints(x, y,
                                  plspec->panel[panel].dataset[idata]->points,
                                  plspec->panel[panel].dataset[idata]->pointLabel,
                                  &plreq->pointLabelSettings, linetype_default);
                  if (plreq->overlay.flags)
                    {
                      free(x);
                      free(y);
                    }

                  draw_sddsplot_strings(plspec->panel[panel].dataset[idata], linetype_default, limit);
                  drawSddsplotLines(plspec->panel[panel].dataset[idata], linetype_default, limit);
                  if (plspec->panel[panel].dataset[idata]->legend)
                    {
                      if (nLegends >= maxLegends)
                        {
                          maxLegends += 10;
                          if (!(legend = SDDS_Realloc(legend, sizeof(*legend) * maxLegends)) ||
                              !(graphic = SDDS_Realloc(graphic, sizeof(*graphic) * maxLegends)) ||
                              !(legendFlags = SDDS_Realloc(legendFlags, sizeof(*legendFlags) * maxLegends)))
                            SDDS_Bomb("Memory allocation failure (plot_sddsplot_data)");
                        }
                      legend[nLegends] = plspec->panel[panel].dataset[idata]->legend;
                      graphic[nLegends] = &plspec->panel[panel].dataset[idata]->graphic;
                      legendFlags[nLegends] = plreq->legend.code;
                      nLegends++;
                    }
                }
            }
        }

      if (!nData || limit[0] >= limit[1] || limit[2] >= limit[3])
        {
          if (!nData)
            {
              fprintf(stderr, "No data for panel %ld, %ld:\n",
                      plspec->panel[panel].ix, plspec->panel[panel].iy);
            }
          if (limit[0] >= limit[1])
            {
              fprintf(stderr, "horizontal scale improperly defined for plotting the following:\n");
              limit[1] = -(limit[0] = -1);
            }
          if (limit[2] >= limit[3])
            {
              fprintf(stderr, "vertical scale improperly defined for plotting the following:\n");
              limit[3] = -(limit[2] = -1);
            }
          for (idata = 0; idata < plspec->panel[panel].datasets; idata++)
            {
              ireq = plspec->panel[panel].dataset[idata]->request_index;
              iname = plspec->panel[panel].dataset[idata]->dataname_index;
              fprintf(stderr, "  %s  vs  %s\n",
                      plspec->plot_request[ireq].xname[iname],
                      plspec->plot_request[ireq].yname[iname]);
            }
          nData = 0;
        }

      if (!(flags & PLREQ_NOLABELS))
        {
          if (!(plspec->panel[panel].joinScaleFlags & JOINSCALE_NOBOT))
            {
              plotTitle(plspec->panel[panel].title[0], 1, flags & PLREQ_TOPTITLE,
                        globalLabelScale * titleScale, titleOffset, titleThickness,
                        titleLineType);
            }
          if (!(plspec->panel[panel].joinScaleFlags & JOINSCALE_NOTOP))
            {
              plotTitle(plspec->panel[panel].title[1], 0, flags & PLREQ_TOPTITLE,
                        globalLabelScale * toplineScale, toplineOffset, toplineThickness,
                        toplineLineType);
            }
        }
      if (flags & PLREQ_DATESTAMP)
        {
          time_date_stamp();
        }

      if (nLegends)
        {
          lspace = plspec->panel[panel].lspace;
          set_legend_space(lspace[0], lspace[1], lspace[2], lspace[3]);
          make_legend(legend, graphic, nLegends, plspec->panel[panel].legend.scale, plspec->panel[panel].legend.thickness, legendFlags, 0);
          nLegends = 0;
        }

      if (!nData)
        {
          double x, y;
          char *noData;
          cp_str(&noData, "$i$iNo data$d$d");
          set_mapping(0.0, 1.0, 0.0, 1.0);
          x = y = 0.5;
          set_linetype(linetype_default);
          jxyplot_string(&x, &y, noData, 'c', 'c');
          free(noData);
        }

      ireqlast = -1;
      numcolors = 0;
      makeintensitybar = 0;
      for (idata = 0; idata < plspec->panel[panel].datasets; idata++)
        {
          ireq = plspec->panel[panel].dataset[idata]->request_index;
          if ((plspec->plot_request[ireq].color_settings.flags & COLORSET_USERDEFINED) && (ireq != ireqlast))
            {
              numcolors += plspec->plot_request[ireq].color_settings.num;
              ireqlast = ireq;
              if ((plspec->plot_request + ireq)->split.name && !((plspec->plot_request + ireq)->split.flags & SPLIT_NOCOLORBAR))
                {
                  plreq = plspec->plot_request + ireq;
                  makeintensitybar = 1;
                }
            }
        }
      numcolors2 = numcolors;
      coloroffset = 0;
      if (makeintensitybar)
        {
          long pen0;
          pen0 = set_linetype(0);
          set_linetype(pen0);
          if ((plreq->split.flags & SPLIT_PARAMETERCHANGE) && plreq->data_class != PARAMETER_DATA)
            {
              /*min_level=plreq->split.min;*/
              min_level = plreq->split.start;
              max_level = plreq->split.min + plreq->split.width * (1 + (long)((plreq->split.max - plreq->split.min) / plreq->split.width));
            }
          if (plreq->split.flags & SPLIT_COLUMNBIN || (plreq->split.flags & SPLIT_PARAMETERCHANGE && plreq->data_class == PARAMETER_DATA))
            {
              min_level = plreq->split.start;
              /*if (min_level==DBL_MAX)
                min_level=limit[2];*/
              /*if (max_level==-DBL_MAX)
                max_level=limit[3];*/
              max_level = plreq->split.end;
            }

          numcolors2 = ((max_level - min_level) / plreq->split.width) + 1;
          if (numcolors2 > 100)
            {
              numcolors2 = ceil(numcolors2 * 100.0 / numcolors);
            }

          if (min_level == DBL_MAX)
            min_level = limit[2];
          if (max_level == -DBL_MAX)
            max_level = limit[3];
          if (plreq->color_settings.flags & COLORSET_SPECTRAL)
            {
              alloc_spectrum(numcolors2, 1, 0, 0, 0, 0, 0, 0);
            }
          else if (plreq->color_settings.flags & COLORSET_RSPECTRAL)
            {
              alloc_spectrum(numcolors2, 2, 0, 0, 0, 0, 0, 0);
            }
          else if (plreq->color_settings.flags & COLORSET_TEMPERATURE)
            {
              alloc_spectrum(numcolors2, 4, 0, 0, 0, 0, 0, 0);
            }
          else if (plreq->color_settings.flags & COLORSET_RTEMPERATURE)
            {
              alloc_spectrum(numcolors2, 3, 0, 0, 0, 0, 0, 0);
            }
          else if (plreq->color_settings.flags & COLORSET_START)
            {
              alloc_spectrum(numcolors2, 0, plreq->color_settings.red[0], plreq->color_settings.green[0], plreq->color_settings.blue[0], plreq->color_settings.red[1], plreq->color_settings.green[1], plreq->color_settings.blue[1]);
            }
          make_intensity_bar(numcolors2 < 101 ? numcolors2 - 1 : 100,
                             coloroffset,
                             0,
                             min_level,
                             max_level,
                             0,
                             1,
                             plreq->intensityBar_settings.flags & INTENSITYBAR_TEXT_GIVEN? plreq->intensityBar_settings.text: plreq->split.symbol,
                             plreq->split.units,
                             linethickness_default,
                             plreq->intensityBar_settings.flags & INTENSITYBAR_LABELSIZE_GIVEN ? plreq->intensityBar_settings.labelsize : 1.0,
                             plreq->intensityBar_settings.flags & INTENSITYBAR_UNITSIZE_GIVEN ? plreq->intensityBar_settings.unitsize : 1.0,
                             plreq->intensityBar_settings.flags & INTENSITYBAR_XADJUST_GIVEN ? plreq->intensityBar_settings.xadjust : 0.0);
          set_linetype(linetype_default);
        }

      frameEnded = 0;
      if (plspec->panel[panel].last_panel_on_page)
        {
          frameEnded = 1;
          frame_end(1);
        }
    } /* loop over panels */
  if (!frameEnded)
    frame_end(1);
  frameEnded = 1;
  (term_tbl[term].movie)();
  free(legend);
  free(graphic);
}

void drawSddsplotLines(PLOT_DATA *dataset, long linetypeDefault,
                       double *limit)
{
  long iline, oldLinetype;
  DRAW_LINE_SPEC *drawLineSpec;
  double x0equiv, x1equiv, y0equiv, y1equiv;
  double x[2], y[2];
  oldLinetype = set_linetype(linetypeDefault);
  drawLineSpec = dataset->drawLineSpec;
  for (iline = 0; iline < dataset->drawLineSpecs; iline++)
    {
      if (drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN)
        {
          set_linetype(drawLineSpec[iline].linetype);
        }
      set_linethickness(drawLineSpec[iline].linethickness);
      if (drawLineSpec[iline].flags & DRAW_LINE_X0GIVEN)
        x0equiv = drawLineSpec[iline].x0;
      else
        {
          x0equiv = (limit[1] - limit[0]) * drawLineSpec[iline].p0 + limit[0];
        }
      if (drawLineSpec[iline].flags & DRAW_LINE_Y0GIVEN)
        y0equiv = drawLineSpec[iline].y0;
      else
        {
          y0equiv = (limit[3] - limit[2]) * drawLineSpec[iline].q0 + limit[2];
        }
      if (drawLineSpec[iline].flags & DRAW_LINE_X1GIVEN)
        x1equiv = drawLineSpec[iline].x1;
      else
        {
          x1equiv = (limit[1] - limit[0]) * drawLineSpec[iline].p1 + limit[0];
        }
      if (drawLineSpec[iline].flags & DRAW_LINE_Y1GIVEN)
        y1equiv = drawLineSpec[iline].y1;
      else
        {
          y1equiv = (limit[3] - limit[2]) * drawLineSpec[iline].q1 + limit[2];
        }
      if (drawLineSpec[iline].flags & DRAW_LINE_CLIPGIVEN)
        {
          x[0] = x0equiv;
          y[0] = y0equiv;
          x[1] = x1equiv;
          y[1] = y1equiv;
          plot_lines(x, y, 2,
                     drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN ? drawLineSpec[iline].linetype : oldLinetype, 0);
        }
      else
        {
          pdraw(x0equiv, y0equiv, 0);
          pdraw(x1equiv, y1equiv, 1);
        }
      if (drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN)
        {
          set_linetype(linetypeDefault);
        }
    }
  set_linetype(oldLinetype);
  set_linethickness(0);
}

void labelDataPoints(double *x, double *y, long n, char **pointLabel, POINTLABEL_SETTINGS *pointLabelSettings, long linetype_default)
{
  long point;
  double x0, y0;
  char xJustify, yJustify;
  double hsize, vsize;
  char *ptr;

  if (!pointLabel)
    return;
  set_linetype(linetype_default);
  get_char_size(&hsize, &vsize, 1);
  char_scale(pointLabelSettings->scale, pointLabelSettings->scale, 0);
  if (pointLabelSettings->flags & POINTLABEL_LINETYPE_GIVEN)
    pointLabelSettings->linetype = set_linetype(pointLabelSettings->linetype);

  set_linethickness(pointLabelSettings->thickness);
  if (pointLabelSettings->flags&POINTLABEL_VERTICAL)
      vertical_print(1);
  for (point = 0; point < n; point++)
    {
      x0 = x[point];
      y0 = y[point];
      if ((ptr = strstr(pointLabel[point], "$j")) && strlen(ptr) == 4)
        {
          /* If the string ends with $jXY, then the justification modes are X and Y */
          *ptr = 0;
          xJustify = *(ptr + 2);
          yJustify = *(ptr + 3);
        }
      else if (pointLabelSettings->justifyMode)
        {
          xJustify = pointLabelSettings->justifyMode[0];
          yJustify = pointLabelSettings->justifyMode[1];
        }
      else
        {
          xJustify = 'l';
          yJustify = 'b';
        }
      jxyplot_string(&x0, &y0, pointLabel[point], xJustify, yJustify);
      /*    if (pointLabelSettings->flags&POINTLABEL_LINETYPE_GIVEN)
            set_linetype(pointLabelSettings->linetype); */
      if (ptr)
        *ptr = '$';
    }
  if (pointLabelSettings->flags & POINTLABEL_LINETYPE_GIVEN)
    set_linetype(linetype_default);
  if (pointLabelSettings->flags&POINTLABEL_VERTICAL)
      vertical_print(0);
  set_char_size(hsize, vsize, 1);
}

void draw_sddsplot_strings(PLOT_DATA *dataset, long linetypeDefault, double *limit)
{
  long ilabel;
  double xlabel0, ylabel0, vsize, hsize, xlabel, ylabel, angle, slant;
  char *ptr1, *ptr2, *label;

  set_clipping(0, 0, 0);
  set_linetype(linetypeDefault);
  for (ilabel = 0; ilabel < dataset->string_labels; ilabel++)
    {
      xlabel0 = dataset->string_label[ilabel].position[0];
      ylabel0 = dataset->string_label[ilabel].position[1];
#if defined(DEBUG)
      fprintf(stderr, "plotting label string %s\n  specified coordinates are %e, %e\n",
              dataset->string_label[ilabel].string,
              xlabel, ylabel);
#endif
      if (dataset->string_label[ilabel].flags & LABEL_P_GIVEN)
        xlabel0 = (limit[1] - limit[0]) * xlabel0 + limit[0];
      if (dataset->string_label[ilabel].flags & LABEL_Q_GIVEN)
        ylabel0 = (limit[3] - limit[2]) * ylabel0 + limit[2];
      if (dataset->string_label[ilabel].flags & LABEL_SCALE_GIVEN)
        char_scale(dataset->string_label[ilabel].scale, dataset->string_label[ilabel].scale, 0);
      angle = slant = 0;
      if (dataset->string_label[ilabel].flags & LABEL_ANGLE_GIVEN)
        angle = dataset->string_label[ilabel].angle;
      if (dataset->string_label[ilabel].flags & LABEL_SLANT_GIVEN)
        slant = dataset->string_label[ilabel].slant;
      if (angle || slant)
        char_angle(angle, slant);
#if defined(DEBUG)
      fprintf(stderr, " plotting at x=%e, y=%e with justification=%s\n",
              xlabel, ylabel,
              dataset->string_label[ilabel].justify_mode ? dataset->string_label[ilabel].justify_mode : "default");
#endif
      get_char_size(&hsize, &vsize, 1);
      if (dataset->string_label[ilabel].flags & LABEL_LINETYPE_GIVEN)
        dataset->string_label[ilabel].linetype = set_linetype(dataset->string_label[ilabel].linetype);
      if (dataset->string_label[ilabel].flags & LABEL_THICKNESS_GIVEN)
        set_linethickness(dataset->string_label[ilabel].thickness);
      if (!SDDS_CopyString(&label, dataset->string_label[ilabel].string))
        {
          SDDS_Bomb("memory allocation failure (draw_sddsplot_strings)");
        }
      ptr1 = label;
      do
        {
          if ((ptr2 = strchr(ptr1, '\n')))
            *ptr2 = 0;
          xlabel = xlabel0;
          ylabel = ylabel0;
          jxyplot_string(&xlabel, &ylabel, ptr1,
                         dataset->string_label[ilabel].justify_mode ? dataset->string_label[ilabel].justify_mode[0] : 'l',
                         dataset->string_label[ilabel].justify_mode ? dataset->string_label[ilabel].justify_mode[1] : 'b');
          ylabel0 -= vsize * 1.5;
          if (ptr2)
            {
              *ptr2 = '\n';
              ptr1 = ptr2 + 1;
            }
        }
      while (ptr2);
      free(label);
      if (dataset->string_label[ilabel].flags & LABEL_SCALE_GIVEN)
        char_scale(1 / dataset->string_label[ilabel].scale, 1 / dataset->string_label[ilabel].scale, 0);
      if (dataset->string_label[ilabel].flags & LABEL_ANGLE_GIVEN ||
          dataset->string_label[ilabel].flags & LABEL_SLANT_GIVEN)
        char_angle(0.0, 0.0);
      if (dataset->string_label[ilabel].flags & LABEL_LINETYPE_GIVEN)
        dataset->string_label[ilabel].linetype = set_linetype(dataset->string_label[ilabel].linetype);
      if (dataset->string_label[ilabel].flags & LABEL_THICKNESS_GIVEN)
        set_linethickness(0);
    }
  set_clipping(1, 1, 1);
}

void sddsplotDataArrays(double *xn, double *yn, double *sxn, double *syn, long npts,
                        double xgap, double ygap, GRAPHIC_SPEC *graphic, unsigned long flags,
                        double *pspace)
{
  long orig_linetype;

  set_clipping(1, 1, 1);
  if (npts == 0)
    return;

  orig_linetype = get_linetype();

  if (graphic->flags & GRAPHIC_CONNECT &&
      (graphic->element == PLOT_SYMBOL || graphic->element == PLOT_ERRORBAR ||
       graphic->element == PLOT_IMPULSE || graphic->element == PLOT_YIMPULSE ||
       graphic->element == PLOT_BAR || graphic->element == PLOT_YBAR))
    {
      if (xgap || ygap)
        plot_lines_gap(xn, yn, xgap, ygap, npts,
                       graphic->connect_linetype == -1 ? graphic->subtype : graphic->connect_linetype, graphic->thickness);
      else if (flags & PLREQ_SEVER)
        plot_lines_sever(xn, yn, npts,
                         graphic->connect_linetype == -1 ? graphic->subtype : graphic->connect_linetype, graphic->thickness);
      else
        plot_lines(xn, yn, npts,
                   graphic->connect_linetype == -1 ? graphic->subtype : graphic->connect_linetype, graphic->thickness);
    }
  switch (graphic->element)
    {
    case PLOT_LINE:
      if (xgap || ygap)
        {
          plot_lines_gap(xn, yn, xgap, ygap, npts, graphic->type, graphic->thickness);
        }
      else if (flags & PLREQ_SEVER)
        {
          plot_lines_sever(xn, yn, npts, graphic->type, graphic->thickness);
        }
      else
        {
          plot_lines(xn, yn, npts, graphic->type, graphic->thickness);
        }
      break;
    case PLOT_SYMBOL:
      plot_points_fill(xn, yn, npts, graphic->type, graphic->subtype,
                       graphic->scale * (pspace[1] - pspace[0]) / 0.75, graphic->thickness, graphic->fill);
      set_linetype(orig_linetype);
      break;
    case PLOT_ERRORBAR:
      set_linetype(graphic->subtype);
      plot_error_bars(xn, yn, sxn, syn, npts, graphic->type, graphic->thickness);
      set_linetype(orig_linetype);
      break;
    case PLOT_DOTS:
      plot_dots(xn, yn, npts, graphic->type, graphic->subtype);
      break;
    case PLOT_IMPULSE:
      set_linetype(graphic->type);
      plot_impulses(xn, yn, npts, graphic->type, graphic->thickness);
      set_linetype(orig_linetype);
      break;
    case PLOT_YIMPULSE:
      set_linetype(graphic->type);
      plot_yimpulses(xn, yn, npts, graphic->type, graphic->thickness);
      set_linetype(orig_linetype);
      break;
    case PLOT_BAR:
      set_linetype(graphic->subtype);
      plot_bars(xn, yn, npts, graphic->type, graphic->thickness);
      set_linetype(orig_linetype);
      break;
    case PLOT_YBAR:
      set_linetype(graphic->subtype);
      plot_ybars(xn, yn, npts, graphic->type, graphic->thickness);
      set_linetype(orig_linetype);
      break;
    case PLOT_ARROW:
      plot_arrows(xn, yn, sxn, syn, npts, &graphic->arrow);
      set_linetype(orig_linetype);
      break;
    }
  pmove(xn[npts - 1], yn[npts - 1]);

  set_linetype(orig_linetype);
}

PLOT_DATA *add_dataset_slots(PLOT_DATA *dataset, long datasets, long datanames)
{
  long i, newNumber;
  /* realloc in 500 slot blocks to avoid SUN realloc problems that lead to N^2 growth in memory use */
  newNumber = 500 * ((datasets + datanames) / 500 + 1);
  dataset = trealloc(dataset, sizeof(*dataset) * (newNumber));
  for (i = datasets; i < datasets + datanames; i++)
    {
      SDDS_ZeroMemory(dataset + i, sizeof(dataset[i]));
      dataset[i].datapage = -1;
    }
  return (dataset);
}

void append_to_dataset(PLOT_DATA *dataset, double *x, char **enumerate,
                       double *y, double *x1, double *y1, double *split_data,
                       char **pointLabel, long points)
{
  long i;
  dataset->x = trealloc(dataset->x, sizeof(*dataset->x) * (dataset->points + points));
  dataset->y = trealloc(dataset->y, sizeof(*dataset->y) * (dataset->points + points));
  if (!x && !y)
    bomb("NULL x and y pointers seen (append_to_dataset)", NULL);
  if (x1)
    dataset->x1 = trealloc(dataset->x1, sizeof(*dataset->x1) * (dataset->points + points));
  if (enumerate)
    dataset->enumerate = trealloc(dataset->enumerate, sizeof(*dataset->enumerate) * (dataset->points + points));
  if (!x)
    {
      if (!enumerate)
        SDDS_Bomb("Internal error: NULL x and enumerate pointers seen (append_to_dataset)");
      if (dataset->points && dataset->enumPlane)
        SDDS_Bomb("Internal error: enumerate plane switched (append_to_dataset)");
      dataset->enumPlane = 0;
    }
  else if (!y)
    {
      if (!enumerate)
        SDDS_Bomb("Internal error: NULL y and enumerate pointers seen (append_to_dataset)");
      if (dataset->points && dataset->enumPlane == 0)
        SDDS_Bomb("Internal error: enumerate plane switched (append_to_dataset)");
      dataset->enumPlane = 1;
    }
  if (pointLabel)
    dataset->pointLabel = trealloc(dataset->pointLabel, sizeof(*dataset->pointLabel) * (dataset->points + points));
  if (y1)
    dataset->y1 = trealloc(dataset->y1, sizeof(*dataset->y1) * (dataset->points + points));
  if (split_data)
    dataset->split_data = trealloc(dataset->split_data, sizeof(*dataset->split_data) * (dataset->points + points));
  for (i = 0; i < points; i++)
    {
      if (enumerate && !x)
        {
          dataset->x[i + dataset->points] = i + dataset->points;
          dataset->enumerate[i + dataset->points] = enumerate[i];
        }
      else
        dataset->x[i + dataset->points] = x[i];
      if (enumerate && !y)
        {
          dataset->y[i + dataset->points] = i + dataset->points;
          dataset->enumerate[i + dataset->points] = enumerate[i];
        }
      else
        dataset->y[i + dataset->points] = y[i];
      if (x1)
        dataset->x1[i + dataset->points] = x1[i];
      if (y1)
        dataset->y1[i + dataset->points] = y1[i];
      if (split_data)
        {
          dataset->split_data[i + dataset->points] = split_data[i];
          if (dataset->split_min > split_data[i])
            dataset->split_min = split_data[i];
          if (dataset->split_max < split_data[i])
            dataset->split_max = split_data[i];
        }
      if (pointLabel)
        dataset->pointLabel[i + dataset->points] = pointLabel[i];
    }
  dataset->points += points;
}

void show_plot_spec(PLOT_SPEC *plspec, FILE *fp)
{
  fprintf(fp, "PLOT_SPEC structure:\n  outputfile = %s\n",
          plspec->outputfile ? plspec->outputfile : "NULL");
  fprintf(fp, "  device = %s\n", plspec->device);
  fprintf(fp, "  layout = %ld by %ld\n", plspec->layout[0], plspec->layout[1]);
  fprintf(fp, "  %ld plot requests\n", plspec->plot_requests);
}

void show_plot_request(PLOT_REQUEST *plreq, long index, FILE *fp)
{
  long i;
  plreq += index;
  fprintf(fp, "PLOT_REQUEST structure %ld:\n  %ld filenames:\n", index, plreq->filenames);
  for (i = 0; i < plreq->filenames; i++)
    fprintf(fp, "    %s\n", plreq->filename[i]);
  fprintf(fp, "  %ld dataname sets:\n", plreq->datanames);
  for (i = 0; i < plreq->datanames; i++)
    fprintf(fp, "    %s  %s  %s  %s\n", plreq->xname[i], plreq->yname[i],
            plreq->x1name[i] ? plreq->x1name[i] : "NULL",
            plreq->y1name[i] ? plreq->y1name[i] : "NULL");
  fprintf(fp, "  GRAPHIC_SPEC:  element=%ld, type=%ld, scale=%e, connect_linetype=%ld, vary=%ld\n",
          plreq->graphic.element, plreq->graphic.type, plreq->graphic.scale, plreq->graphic.connect_linetype,
          plreq->graphic.vary);
  fprintf(fp, "  ZOOM_SPEC: mag=%e, %e  center=%e, %e  flags=%lx\n",
          plreq->zoom.magnification[0], plreq->zoom.magnification[1],
          plreq->zoom.center[0], plreq->zoom.center[1], plreq->zoom.flags);
  fprintf(fp, "  standard labels:\n");
  for (i = 0; i < 4; i++)
    fprintf(fp, "    label=%s  offset=%e  scale=%e  flags=%lx\n",
            plreq->label[i].label ? plreq->label[i].label : "NULL",
            plreq->label[i].offset, plreq->label[i].scale,
            plreq->label[i].flags);
  fprintf(fp, "  %ld string labels:\n", plreq->string_labels);
  for (i = 0; i < plreq->string_labels; i++)
    fprintf(fp, "    string=%s  position=%e, %e  scale=%e  flags=%lx\n",
            plreq->string_label[i].string, plreq->string_label[i].position[0],
            plreq->string_label[i].position[1], plreq->string_label[i].scale,
            plreq->string_label[i].flags);
  fprintf(fp, "  %ld lines:\n", plreq->drawLineSpecs);
  for (i = 0; i < plreq->drawLineSpecs; i++)
    {
      fprintf(fp, "    %ld: x0=%e, y0=%e, p0=%e, q0=%e\n",
              i,
              plreq->drawLineSpec[i].x0,
              plreq->drawLineSpec[i].y0,
              plreq->drawLineSpec[i].p0,
              plreq->drawLineSpec[i].q0);
      fprintf(fp, "    %ld: x1=%e, y1=%e, p1=%e, q1=%e\n",
              i,
              plreq->drawLineSpec[i].x1,
              plreq->drawLineSpec[i].y1,
              plreq->drawLineSpec[i].p1,
              plreq->drawLineSpec[i].q1);
    }

  if (plreq->legend.value)
    fprintf(fp, "  LEGEND_SPEC:  value=%s  edit_command=%s  code=%lx\n",
            plreq->legend.value, plreq->legend.edit_command, plreq->legend.code);
  fprintf(fp, "  OVERLAY_SPEC: factor=%e, %e  offset=%e, %e  flags=%lx\n",
          plreq->overlay.factor[0], plreq->overlay.factor[1],
          plreq->overlay.offset[0], plreq->overlay.offset[1], plreq->overlay.flags);
  fprintf(fp, "  flags = %lx   mode = %lx\n", plreq->flags, plreq->mode);
  if (plreq->split.name)
    fprintf(fp, "  SPLIT_SPEC:  name=%s  interval=%" PRId32 "  width=%e  flags=%lx\n",
            plreq->split.name, plreq->split.interval, plreq->split.width, plreq->split.flags);
  fprintf(fp, "  %ld filters\n", plreq->filters);
  for (i = 0; i < plreq->filters; i++)
    {
      fprintf(fp, "   Filter %ld\n", i);
      show_filter(fp, plreq->filter[i]);
    }
  fprintf(fp, "  %ld time filters\n", plreq->time_filters);
  for (i = 0; i < plreq->time_filters; i++)
    {
      fprintf(fp, "   Time Filter %ld\n", i);
      fprintf(stderr, "Time filter: <%s> %22.15e, %22.15e\n", plreq->time_filter[i]->name, plreq->time_filter[i]->before, plreq->time_filter[i]->after);
    }
  fprintf(fp, "  %ld matches\n", plreq->matches);
  fprintf(fp, "  sparse: interval=%ld  offset=%ld\n",
          plreq->sparse_interval, plreq->sparse_offset);
  fprintf(fp, "  clip:  head=%ld  tail=%ld\n", plreq->clip_head, plreq->clip_tail);
  fprintf(fp, "  sample_fraction=%e\n", plreq->sample_fraction);
  fprintf(fp, "  stagger:  %e, %e\n",
          plreq->stagger_amount[0], plreq->stagger_amount[1]);
  fprintf(fp, "  aspect_ratio = %e\n", plreq->aspect_ratio);
  fprintf(fp, "  mapping = %e, %e by %e, %e\n",
          plreq->mapping[0], plreq->mapping[1], plreq->mapping[2], plreq->mapping[3]);
  fprintf(fp, "  pspace = %e, %e by %e, %e\n",
          plreq->pspace[0], plreq->pspace[1], plreq->pspace[2], plreq->pspace[3]);
  fprintf(fp, "  lspace = %e, %e by %e, %e\n",
          plreq->lspace[0], plreq->lspace[1], plreq->lspace[2], plreq->lspace[3]);
  fprintf(fp, "  alignment: flags = %ld, pin = %e, %e\n",
          plreq->alignSettings.flags, plreq->alignSettings.pinUnitSpace[0],
          plreq->alignSettings.pinUnitSpace[1]);
}

void show_filter(FILE *fp, FILTER_DEFINITION *filter)
{
  long i;

  for (i = 0; i < filter->filter_terms; i++)
    {
      fprintf(fp, "  %s filter %s on %.15e, %.15e\n",
              filter->is_parameter ? "Parameter" : "Column",
              filter->filter_term[i].name,
              filter->filter_term[i].lower,
              filter->filter_term[i].upper);
    }
}

void add_plot_request(PLOT_SPEC *plspec)
{
  PLOT_REQUEST *newReq, *firstReq;
  if (plspec->plot_requests == 0 || !plspec->plot_request)
    {
      /* add plot request default element */
      plspec->plot_request = (PLOT_REQUEST *)calloc(sizeof(*plspec->plot_request), 1);
      plspec->plot_request[plspec->plot_requests].graphic.connect_linetype = -1;
      plspec->plot_request[plspec->plot_requests].graphic.element = PLOT_LINE;
      plspec->plot_requests = 1;
      SDDS_CopyString(&plspec->plot_request[0].scalesGroupSpec[0].ID,
                      RESERVED_SCALESGROUP_ID);
      SDDS_CopyString(&plspec->plot_request[0].scalesGroupSpec[1].ID,
                      RESERVED_SCALESGROUP_ID);
    }
  else
    {
      plspec->plot_request = SDDS_Realloc(plspec->plot_request,
                                          sizeof(*plspec->plot_request) * (plspec->plot_requests + 1));
      firstReq = plspec->plot_request;
      newReq = plspec->plot_request + plspec->plot_requests;
      memcpy((char *)newReq, (char *)firstReq, sizeof(*plspec->plot_request));
      newReq->filename = NULL;
      newReq->filenames = 0;
      if (firstReq->filenames)
        {
          if (!(newReq->filename = SDDS_Malloc(sizeof(*newReq->filename) * firstReq->filenames)) ||
              !SDDS_CopyStringArray(newReq->filename, firstReq->filename, firstReq->filenames))
            SDDS_Bomb("Memory allocation/string copy failure (add_plot_request)");
          newReq->filenames = firstReq->filenames;
        }
      SDDS_CopyString(&newReq->scalesGroupSpec[0].ID,
                      RESERVED_SCALESGROUP_ID);
      SDDS_CopyString(&newReq->scalesGroupSpec[1].ID,
                      RESERVED_SCALESGROUP_ID);
      if (firstReq->filters)
        {
          newReq->filter = (FILTER_DEFINITION **)calloc(sizeof(FILTER_DEFINITION *),
                                                        firstReq->filters);
          memcpy((char *)newReq->filter, (char *)firstReq->filter,
                 firstReq->filters * sizeof(FILTER_DEFINITION *));
          newReq->match = (MATCH_DEFINITION **)calloc(sizeof(MATCH_DEFINITION *),
                                                      firstReq->matches);
          memcpy((char *)newReq->match, (char *)firstReq->match,
                 firstReq->matches * sizeof(MATCH_DEFINITION *));
        }
      if (firstReq->time_filters)
        {
          newReq->time_filter = (TIME_FILTER_DEFINITION **)calloc(sizeof(TIME_FILTER_DEFINITION *),
                                                                  firstReq->time_filters);
          memcpy((char *)newReq->time_filter, (char *)firstReq->time_filter,
                 firstReq->time_filters * sizeof(TIME_FILTER_DEFINITION *));
        }
      if (plspec->plot_request[plspec->plot_requests].graphic.type == 0)
        {
          plspec->plot_request[plspec->plot_requests].graphic.type = plspec->plot_request[plspec->plot_requests].linetype_default;
        }
      if (plspec->plot_request[plspec->plot_requests].graphic.thickness == 0)
        {
          plspec->plot_request[plspec->plot_requests].graphic.thickness = plspec->plot_request[plspec->plot_requests].linethickness_default;
        }
      if (plspec->plot_request[plspec->plot_requests].global_thickness_default)
        {
          long i;
          /* legend */
          if (!plspec->plot_request[plspec->plot_requests].legend.thickness)
            {
              plspec->plot_request[plspec->plot_requests].legend.thickness =
                plspec->plot_request[plspec->plot_requests].global_thickness_default;
            }
          /* labels */
          for (i = 0; i < 4; i++)
            if (!(plspec->plot_request[plspec->plot_requests].label[i].flags & LABEL_THICKNESS_GIVEN))
              {
                plspec->plot_request[plspec->plot_requests].label[i].thickness =
                  plspec->plot_request[plspec->plot_requests].global_thickness_default;
                plspec->plot_request[plspec->plot_requests].label[i].flags |= LABEL_THICKNESS_GIVEN;
              }
          /* strings */
          for (i = 0; i < plspec->plot_request[plspec->plot_requests].string_labels; i++)
            if (!(plspec->plot_request[plspec->plot_requests].string_label[i].flags & LABEL_THICKNESS_GIVEN))
              {
                plspec->plot_request[plspec->plot_requests].string_label[i].thickness =
                  plspec->plot_request[plspec->plot_requests].global_thickness_default;
                plspec->plot_request[plspec->plot_requests].string_label[i].flags |= LABEL_THICKNESS_GIVEN;
              }
          /* ticks */
          for (i = 0; i < 2; i++)
            {
              if (!(plspec->plot_request[plspec->plot_requests].tick_settings.flags &
                    (TICKSET_XTHICKNESS << i)))
                {
                  plspec->plot_request[plspec->plot_requests].tick_settings.thickness[i] =
                    plspec->plot_request[plspec->plot_requests].global_thickness_default;
                  plspec->plot_request[plspec->plot_requests].tick_settings.flags |=
                    TICKSET_XTHICKNESS << i;
                }
              if (!(plspec->plot_request[plspec->plot_requests].tick_settings.labelThickness[i]))
                {
                  plspec->plot_request[plspec->plot_requests].tick_settings.labelThickness[i] =
                    plspec->plot_request[plspec->plot_requests].global_thickness_default;
                }
            }
          /* subticks */
          for (i = 0; i < 2; i++)
            {
              if (!(plspec->plot_request[plspec->plot_requests].subtick_settings.flags &
                    (TICKSET_XTHICKNESS << i)))
                {
                  plspec->plot_request[plspec->plot_requests].subtick_settings.thickness[i] =
                    plspec->plot_request[plspec->plot_requests].global_thickness_default;
                  plspec->plot_request[plspec->plot_requests].subtick_settings.flags |=
                    TICKSET_XTHICKNESS << i;
                }
            }
        }
      plspec->plot_requests++;
    }
}

/* routine: shorten_filename()
 * purpose: shorten a filename by eliminating the directory specification.
 *          Also converts to lower case (VAX only).
 */

void shorten_filename(char *s)
{
  char *ptr;

  if ((ptr = strchr(s, ']')))
    strcpy_ss(s, ptr + 1);
  else if ((ptr = strchr(s, '>')))
    strcpy_ss(s, ptr + 1);
#ifdef VAX_VMS
  str_tolower(s);
#endif
}

/* routine: cleanup_label()
 * purpose: eliminate leading and trailing space characters (space, tab, newline ,etc)
 *          in a string.  Also, truncate at 132 characters.
 */

void cleanup_label(char *s)
{
  char *ptr;

  ptr = s;
  while (isspace(*ptr))
    ptr++;
  strcpy_ss(s, ptr);

  ptr = s + strlen(s) - 1;
  while (ptr >= s && isspace(*ptr))
    *ptr-- = 0;

  if ((long)strlen(s) > 132)
    strcpy_ss(s + 132 - 4, "...");
}

#if defined(COMPILE_AS_SUBROUTINE)
void putenv_label(char *name, char *value)
{
  char *buffer;
  buffer = tmalloc(sizeof(*buffer) * (strlen(name) + strlen(value) + 2));
  sprintf(buffer, "%s=%s", name, value);
  putenv(buffer);
}
#endif

long add_filename(PLOT_SPEC *plotspec, char *filename)
{
  PLOT_REQUEST *plreq;
  long i;

  plreq = &plotspec->plot_request[plotspec->plot_requests - 1];
  for (i = 0; i < plreq->filenames; i++)
    {
      if (strcmp(filename, plreq->filename[i]) == 0)
        return 1;
    }
  plreq->filename = SDDS_Realloc(plreq->filename, sizeof(*plreq->filename) * (plreq->filenames + 1));
  SDDS_CopyString(&plreq->filename[plreq->filenames], filename);
  plreq->filenames += 1;
  return 1;
}

long count_chars(char *string, char c)
{
  long i = 0;
  while (*string)
    {
      if (*string++ == c)
        i++;
    }
  return i;
}

long adjust_sddsplot_overlay_data(double *x, double *u, long n_pts, double range, double min,
                                  OVERLAY_SPEC *overlay, long shift)
{
  long i;
  double umin, umax, urange, coffset;

  umin = umax = 0;
  find_min_max(&umin, &umax, u, n_pts);
  if (umin == umax)
    {
      fprintf(stderr, "warning: range of data is zero--can't overlay (adjust_sddsplot_overlay_data)\n");
      return 0;
    }
  urange = (umax - umin);

#ifdef DEBUG
  fprintf(stderr, "overlay with shift %ld, range=%e, min=%e\n",
          shift, range, min);
  fprintf(stderr, "  bits:  normal=%c  center=%c  factor=%c  offset=%c  unit-offset=%c\n",
          (overlay->flags & (OVERLAY_NORMAL << shift)) ? 'y' : 'n',
          (overlay->flags & (OVERLAY_CENTER << shift)) ? 'y' : 'n',
          (overlay->flags & (OVERLAY_FACTOR << shift)) ? 'y' : 'n',
          (overlay->flags & (OVERLAY_OFFSET << shift)) ? 'y' : 'n',
          (overlay->flags & (OVERLAY_UNITOFFSET << shift)) ? 'y' : 'n');
  fprintf(stderr, "factor = %e, offset = %e, unitOffset = %e\n",
          overlay->factor[shift], overlay->offset[shift],
          overlay->unitOffset[shift]);
  fprintf(stderr, "data range: %e, %e\n", umin, umax);
#endif

  if (overlay->flags & (OVERLAY_NORMAL << shift))
    {
      /* start with data unscaled */
      for (i = 0; i < n_pts; i++)
        x[i] = u[i];
    }
  else
    {
      /* scale data so that its full range is the full span of the dimension */
      for (i = 0; i < n_pts; i++)
        x[i] = range * u[i] / urange;
      umin = range * umin / urange;
      umax = range * umax / urange;
      urange = range;
    }

  if (overlay->flags & (OVERLAY_CENTER << shift))
    {
      /* center the data around the center of the dimension */
      coffset = -(umin + umax) / 2 + (2 * min + range) / 2;
      for (i = 0; i < n_pts; i++)
        x[i] += coffset;
      umin += coffset;
      umax += coffset;
    }

  if (overlay->flags & (OVERLAY_FACTOR << shift))
    {
      /* scale the data by a factor about the center */
      if (overlay->flags & (OVERLAY_CENTER << shift))
        coffset = (2 * min + range) / 2;
      else
        coffset = 0;
      for (i = 0; i < n_pts; i++)
        x[i] = (x[i] - coffset) * overlay->factor[shift] + coffset;
      umin *= overlay->factor[shift];
      umax *= overlay->factor[shift];
      urange *= overlay->factor[shift];
    }

  if (overlay->flags & (OVERLAY_UNITALIGN << shift))
    {
      /* offset the data so that 0 corresponds to unit-coordinate of 0 */
      coffset = min;
      for (i = 0; i < n_pts; i++)
        x[i] += coffset;
    }

  if (overlay->flags & (OVERLAY_OFFSET << shift))
    {
      /* offset the data by a value */
      for (i = 0; i < n_pts; i++)
        x[i] += overlay->offset[shift];
    }

  if (overlay->flags & (OVERLAY_UNITOFFSET << shift))
    {
      /* offset the data by a unit-coordinate value */
      for (i = 0; i < n_pts; i++)
        x[i] += range * overlay->unitOffset[shift];
    }

  return 1;
}

long find_datanames_absent(SDDS_TABLE *table, short **dataname_absent, long *datanames_absent,
                           char **yname, char **y1name, char **xname, char **x1name,
                           long candidate_names, long data_class,
                           char **xExcludeName, short *was_wildxExclude, long xExcludeNames,
                           char **yExcludeName, short *was_wildyExclude, long yExcludeNames)
{
  long candidate, nomatch, j, match;

  *dataname_absent = trealloc(*dataname_absent, sizeof(**dataname_absent) * candidate_names);
  *datanames_absent = nomatch = 0;

  for (candidate = 0; candidate < candidate_names; candidate++)
    {
      (*dataname_absent)[candidate] = 0;
      switch (data_class)
        {
        case COLUMN_DATA:
          match = 0;
          for (j = 0; j < xExcludeNames; j++)
            {
              if (was_wildxExclude[j])
                {
                  if (wild_match(xname[candidate], xExcludeName[j]) ||
                      (x1name[candidate] && wild_match(x1name[candidate], xExcludeName[j])))
                    match = 1;
                }
              else if (!strcmp(xname[candidate], xExcludeName[j]) ||
                       (x1name[candidate] && !strcmp(x1name[candidate], xExcludeName[j])))
                match = 1;
              if (match)
                {
                  (*dataname_absent)[candidate] |= 16;
                  break;
                }
            }
          if (!match)
            {
              for (j = 0; j < yExcludeNames; j++)
                {
                  if (was_wildyExclude[j])
                    {
                      if (wild_match(yname[candidate], yExcludeName[j]) ||
                          (y1name[candidate] && wild_match(y1name[candidate], yExcludeName[j])))
                        match = 1;
                    }
                  else if (!strcmp(yname[candidate], yExcludeName[j]) ||
                           (y1name[candidate] && !strcmp(y1name[candidate], yExcludeName[j])))
                    match = 1;
                  if (match)
                    {
                      (*dataname_absent)[candidate] |= 32;
                      break;
                    }
                }
            }
          if ((*dataname_absent)[candidate])
            {
              *datanames_absent += 1;
              nomatch = 1;
            }
          else
            {
              if (SDDS_GetColumnIndex(table, yname[candidate]) < 0)
                (*dataname_absent)[candidate] |= 1;
              if (y1name[candidate] && SDDS_GetColumnIndex(table, y1name[candidate]) < 0)
                (*dataname_absent)[candidate] |= 2;
              if ((xname[candidate] != NULL) && (SDDS_GetColumnIndex(table, xname[candidate]) < 0))
              (*dataname_absent)[candidate] |= 4;
              if (x1name[candidate] && SDDS_GetColumnIndex(table, x1name[candidate]) < 0)
                (*dataname_absent)[candidate] |= 8;
              if ((*dataname_absent)[candidate])
                {
                  *datanames_absent += 1;
                  nomatch = 1;
                }
            }
          break;
        case PARAMETER_DATA:
          if (SDDS_GetParameterIndex(table, yname[candidate]) < 0)
            (*dataname_absent)[candidate] |= 1;
          if (y1name[candidate] && SDDS_GetParameterIndex(table, y1name[candidate]) < 0)
            (*dataname_absent)[candidate] |= 2;
          if ((xname[candidate] != NULL) && (SDDS_GetParameterIndex(table, xname[candidate]) < 0))
            (*dataname_absent)[candidate] |= 4;
          if (x1name[candidate] && SDDS_GetParameterIndex(table, x1name[candidate]) < 0)
            (*dataname_absent)[candidate] |= 8;
          if ((*dataname_absent)[candidate])
            {
              *datanames_absent += 1;
              nomatch = 1;
            }
          break;
        case ARRAY_DATA:
          if (SDDS_GetArrayIndex(table, yname[candidate]) < 0)
            (*dataname_absent)[candidate] |= 1;
          if (y1name[candidate] && SDDS_GetArrayIndex(table, y1name[candidate]) < 0)
            (*dataname_absent)[candidate] |= 2;
          if ((xname[candidate] != NULL) && (SDDS_GetArrayIndex(table, xname[candidate]) < 0))
            (*dataname_absent)[candidate] |= 4;
          if (x1name[candidate] && SDDS_GetArrayIndex(table, x1name[candidate]) < 0)
            (*dataname_absent)[candidate] |= 8;
          if ((*dataname_absent)[candidate])
            {
              *datanames_absent += 1;
              nomatch = 1;
            }
          break;
        default:
          break;
        }
    }
  return !nomatch;
}

double perform_normalization(double *data, double *data1, long points)
{
  double min, max;
  long i;

  find_min_max(&min, &max, data, points);
  if (min == max)
    {
      for (i = 0; i < points; i++)
        data[i] = 0;
      return min;
    }
  min = fabs(min);
  max = fabs(max);
  if (min > max)
    max = min;
  if (max == 0)
    {
      for (i = 0; i < points; i++)
        data[i] = 0;
      return max;
    }
  for (i = 0; i < points; i++)
    {
      data[i] /= max;
      if (data1)
        data1[i] /= max;
    }
  return max;
}

void makeEnumeratedScale(long plane, char **enumerate, double *enumPosition, long enumerates,
                         ENUMERATE_SETTINGS *enum_settings,
                         double position1, double position2, double allowedSpace,
                         long adjustForTicks, double tickFraction,
                         long lineThickness, long labelThickness,
                         char *labelString, long labelLineType)
{
  long i, interval, verticalPrint, singleScale, nEnumLabels;
  long oldThickness;
  double mapping[4], x, y, factor, range;
  double hsize, vsize, hsave, vsave, maxHSize, maxVSize;
  double dummy;
  char jx, jy;

  get_char_size(&hsave, &vsave, 1);
  if (enum_settings->flags & ENUM_SCALEGIVEN)
    {
      get_char_size(&hsize, &vsize, 1);
      hsize *= enum_settings->scale;
      vsize *= enum_settings->scale;
      set_char_size(hsize, vsize, 1);
    }
  if (tickFraction <= 0)
    tickFraction = plane ? 0.0125 : 0.02;

  interval = enum_settings->flags & ENUM_INTERVALGIVEN ? enum_settings->interval : 1;
  if (enum_settings->flags & ENUM_LIMITGIVEN)
    {
      interval = enumerates / enum_settings->limit + 0.5;
      if (interval == 0)
        interval = 1;
    }

  singleScale = (position1 == position2);
  if (adjustForTicks)
    {
      adjustPositionAndSpaceForTicks(&position1, &allowedSpace,
                                     tickFraction, plane, 0);
    }

  verticalPrint = 0;
  vertical_print(0);
  if (plane == 0 && !(enum_settings->flags & ENUM_ROTATE))
    {
      verticalPrint = 1;
      vertical_print(1);
    }

  get_mapping(mapping + 0, mapping + 1, mapping + 2, mapping + 3);
  maxHSize = 0;
  maxVSize = 0;
  range = plane ? mapping[1] - mapping[0] : mapping[3] - mapping[2];
  nEnumLabels = 0;
  for (i = 0; i < enumerates; i++)
    {
      if (!(enum_settings->flags & ENUM_ALLTICKS) && i % interval)
        continue;
      make_ticks(plane ? 'y' : 'x', enumPosition[i],
                 1, 1, position1, tickFraction * range, 0, lineThickness);
      if (!singleScale)
        make_ticks(plane ? 'y' : 'x', enumPosition[i],
                   1, 1, position2, -tickFraction * range, 0, lineThickness);
      nEnumLabels++;
      computePlottedStringSize(enumerate[i], &hsize, &vsize, &dummy, &dummy, 0);

      if (hsize > maxHSize)
        maxHSize = hsize;
      if (vsize > maxVSize)
        maxVSize = vsize;
    }

  oldThickness = set_linethickness(lineThickness);
  if (plane)
    {
      pmove(position1, mapping[2]);
      pdraw(position1, mapping[3], 1);
      if (!singleScale)
        {
          pmove(position2, mapping[2]);
          pdraw(position2, mapping[3], 1);
        }
    }
  else
    {
      pmove(mapping[0], position1);
      pdraw(mapping[1], position1, 1);
      if (!singleScale)
        {
          pmove(mapping[0], position2);
          pdraw(mapping[1], position2, 1);
        }
    }

  if (maxHSize == 0 || maxVSize == 0)
    {
      vertical_print(0);
      set_char_size(hsave, vsave, 1);
      return;
    }

  set_linethickness(labelThickness);
  get_char_size(&hsize, &vsize, 1);
  if (plane)
    {
      jx = 'r';
      jy = 'b';
      /* scale to use the available space horizontally */
      factor = maxHSize / (allowedSpace * 0.9);
      if (enum_settings->flags & ENUM_SCALEGIVEN)
        {
          if (hsize > hsize / factor)
            hsize /= factor;
          if (vsize > vsize / factor)
            vsize /= factor;
          if (maxHSize > maxHSize / factor)
            maxHSize /= factor;
          if (maxVSize > maxVSize / factor)
            maxVSize /= factor;
        }
      else
        {
          hsize /= factor;
          vsize /= factor;
          maxHSize /= factor;
          maxVSize /= factor;
          if ((factor = maxVSize / ((enumerates * 0.35) / nEnumLabels)) > 1)
            {
              /* scale down to avoid running labels together */
              hsize /= factor;
              vsize /= factor;
              maxHSize /= factor;
              maxVSize /= factor;
            }
        }
      position1 -= allowedSpace * 0.05;
    }
  else
    {
      /* scale to use the available space vertically */
      factor = maxVSize / (allowedSpace * 0.9);
      if (enum_settings->flags & ENUM_SCALEGIVEN)
        {
          /* The allowedSpace is invalid for vertical labels.
             if (hsize > hsize / factor)
             hsize /= factor;
             if (vsize > vsize / factor)
             vsize /= factor;
             if (maxHSize > maxHSize / factor)
             maxHSize /= factor;
             if (maxVSize > maxVSize / factor)
             maxVSize /= factor;
          */
        }
      else
        {
          hsize /= factor;
          vsize /= factor;
          maxHSize /= factor;
          maxVSize /= factor;
          if ((factor = (maxHSize * nEnumLabels) / (enumerates * 0.85)) > 1)
            {
              /* scale down to avoid running labels together */
              hsize /= factor;
              vsize /= factor;
              maxHSize /= factor;
              maxVSize /= factor;
            }
        }
      if (verticalPrint)
        {
          jx = 'r';
          jy = 'c';
        }
      else
        {
          jx = 'c';
          jy = 't';
        }
      position1 -= allowedSpace * 0.05;
    }

  if (plane) {
    fix_char_size(hsize, vsize, 1, 4);
  } else {
    fix_char_size(hsize, vsize, 1, 3);
  }
  for (i = 0; i < enumerates; i++)
    {
      if (plane)
        {
          y = enumPosition[i];
          x = position1;
        }
      else
        {
          x = enumPosition[i];
          y = position1;
        }
      if (i % interval == 0)
        {
          jxyplot_string(&x, &y, enumerate[i], jx, jy);
        }
    }
  vertical_print(0);
  set_linethickness(oldThickness);
  set_char_size(hsave, vsave, 1);
}

/*Used by sddscontour*/
void make_enumerated_xscale(ENUMERATE_SETTINGS *enum_settings)
{
  long i, interval;
  double mapping[4], x, y, ysave;
  double hsize, vsize, hsave, vsave;
  char jx, jy;

  get_char_size(&hsave, &vsave, 1);
  hsize = hsave;
  vsize = vsave;

  get_mapping(mapping + 0, mapping + 1, mapping + 2, mapping + 3);
  interval = enum_settings->flags & ENUM_INTERVALGIVEN ? enum_settings->interval : 1;

  if (enum_settings->flags & ENUM_SCALEGIVEN)
    {
      hsize *= enum_settings->scale;
      vsize *= enum_settings->scale;
      set_char_size(hsize, vsize, 1);
    }

  if (enum_settings->flags & ENUM_LIMITGIVEN)
    {
      interval = enum_settings->enumerates / enum_settings->limit + 0.5;
      if (interval == 0)
        interval = 1;
    }

  if (!(enum_settings->flags & ENUM_ROTATE))
    {
      vertical_print(1);
      jx = 'r';
      jy = 'c';
      ysave = mapping[2] - 0.25 * vsize;
    }
  else
    {
      jx = 'c';
      jy = 't';
      ysave = mapping[2] - 0.5 * vsize;
    }

  for (i = 0; i < enum_settings->enumerates; i++)
    {
      if (!(enum_settings->flags & ENUM_ALLTICKS) && i % interval)
        continue;
      x = enum_settings->enumerateIndex[i];
      y = ysave;
      make_ticks('x', x, 1, 1, mapping[2], 0.02 * (mapping[3] - mapping[2]), 0, 0);
      make_ticks('x', x, 1, 1, mapping[3], -0.02 * (mapping[3] - mapping[2]), 0, 0);
      if (i % interval == 0)
        jxyplot_string(&x, &y, enum_settings->enumerate[i], jx, jy);
    }
  vertical_print(0);
  set_char_size(hsave, vsave, 1);
}

/*Used by sddscontour*/
void make_enumerated_yscale(ENUMERATE_SETTINGS *enum_settings)
{
  double hsave, vsave, hsize, vsize;
  double xmin, xmax, ymin, ymax;
  double x, y, yrange, xrange, xprange;
  double pmin, pmax, qmin, qmax;
  long i, maxlen, labels, interval;
  char **label;

  label = enum_settings->enumerate;
  labels = enum_settings->enumerates;
  interval = enum_settings->flags & ENUM_INTERVALGIVEN ? enum_settings->interval : 1;

  get_mapping(&xmin, &xmax, &ymin, &ymax);
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  yrange = ymax - ymin;
  xrange = xmax - xmin;

  get_char_size(&hsave, &vsave, 1);
  vsize = vsave;
  hsize = hsave;

  if (yrange < (labels * 1.5 * vsize) / interval)
    {
      vsize = interval * yrange / (labels * 1.5);
      hsize = hsave / vsave * vsize;
      set_char_size(hsize, vsize, 1);
    }

  for (i = maxlen = 0; i < labels; i += interval)
    if ((long)strlen(label[i]) > maxlen)
      maxlen = strlen(label[i]);

  xprange = xrange / (pmax - pmin) * pmin * 0.8;
  if (xprange < maxlen * hsize)
    {
      hsize = xprange / maxlen;
      vsize = vsave / hsave * hsize;
      set_char_size(hsize, vsize, 1);
    }

  if (enum_settings->flags & ENUM_SCALEGIVEN)
    {
      hsize *= enum_settings->scale;
      vsize *= enum_settings->scale;
      set_char_size(hsize, vsize, 1);
    }

  widen_window(1);
  for (i = 0; i < labels; i++)
    {
      if (!(enum_settings->flags & ENUM_ALLTICKS) && i % interval)
        continue;
      x = xmin - hsize * 0.5;
      y = enum_settings->enumerateIndex[i];
      make_ticks('y', y, 1, 1, xmin, 0.02 * xrange, 0, 0);
      make_ticks('y', y, 1, 1, xmax, -0.02 * xrange, 0, 0);
      if (i % interval == 0)
        jplot_string(&x, &y, label[i], 'r');
    }
  widen_window(0);

  set_char_size(hsave, vsave, 1);
}

void scaleValues(double factor, ...)
{
  va_list argptr;
  double *dptr;
  va_start(argptr, factor);
  while ((dptr = va_arg(argptr, double *)))
    {
      *dptr *= factor;
    }
  va_end(argptr);
}

void make_legend(char **legend, GRAPHIC_SPEC **graphic, long n, double legendScale, long legendThickness, unsigned long *legendFlags, long do_box)
{
  long i, j, legendNumber;
  double h_save, v_save, hSize, vSize, maxHSize, maxVSize, factor;
  double xmin, xmax, ymin, ymax;
  double x[2], y[2], sx[2], sy[2], yrange, xrange;
  double pmin, pmax, qmin, qmax;
  double dummy;
  double lpmin, lpmax, lqmin, lqmax, asf, legend_xrange, legend_yrange;
  long n_legends, orig_linetype;
  static short *makeLegend = NULL;
  static char *buffer = NULL;
  static long bufferSize = 0, maximum_n = 0;

  get_char_size(&h_save, &v_save, 1); /* save character size in user's coordinates */

  get_mapping(&xmin, &xmax, &ymin, &ymax);
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  if (!(yrange = ymax - ymin))
    bomb("y range is zero (make_legend)", NULL);
  if (!(xrange = xmax - xmin))
    bomb("x range is zero (make_legend)", NULL);
  if (pmin == pmax || qmin == qmax)
    bomb("pspace invalid", NULL);
  if (n > maximum_n)
    makeLegend = SDDS_Realloc(makeLegend, sizeof(*makeLegend) * (maximum_n = n));
  maxHSize = maxVSize = 0;

  for (i = n_legends = 0; i < n; i++)
    {
      makeLegend[i] = 0;
      if (legend[i] && !is_blank(legend[i]))
        {
          for (j = 0; j < i; j++)
            if (makeLegend[j] &&
                graphic[i]->element == graphic[j]->element &&
                graphic[i]->type == graphic[j]->type &&
                graphic[i]->subtype == graphic[j]->subtype &&
                legend[j] && strcmp(legend[i], legend[j]) == 0)
              break;
          if (j == i)
            {
              n_legends++;
              makeLegend[i] = 1;
            }
        }
    }
  for (i = 0; i < n; i++)
    {
      if (makeLegend[i])
        {
          computePlottedStringSize(legend[i], &hSize, &vSize, &dummy, &dummy, 0);
          if (hSize > maxHSize)
            maxHSize = hSize;
          if (vSize > maxVSize)
            maxVSize = vSize;
        }
    }
  if (n_legends == 0 || maxHSize == 0 || maxVSize == 0)
    return;

  /* get legend space, which specifies the region for legends
   * in (0,1)x(0,1) == (pmin,pmax)x(qmin,qmax) coordinates 
   */
  get_legend_space(&lpmin, &lpmax, &lqmin, &lqmax);
  legend_xrange = xrange * (lpmax - lpmin);
  legend_yrange = yrange * (lqmax - lqmin);

  /* Adjust character size */
  /* Increase horizontal size to fill the available space */
  get_char_size(&hSize, &vSize, 1);
  factor = (0.95 * legend_xrange) / maxHSize;
  scaleValues(factor, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
  /* Check vertical size. Allow no legend to take up more than
   * 20% of the vertical range */
  if ((factor = (legend_yrange / n_legends / 3.0) / maxVSize) < 1)
    scaleValues(factor, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
  if ((factor = (legend_yrange * 0.05) / maxVSize) < 1)
    scaleValues(factor, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
  if (legendScale && legendScale != 1)
    {
      /* Let the user scale it. */
      scaleValues(legendScale, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
      /* See if we still have something sensible vertically. */
      if ((factor = (legend_yrange / n_legends / 3.0) / maxVSize) > 5 || factor < 0.02)
        scaleValues(factor, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
      /* Enforce horizontal limits strictly as we really know the label size here */
      if ((factor = (0.95 * legend_xrange) / maxHSize) < 1)
        scaleValues(factor, &maxHSize, &maxVSize, &hSize, &vSize, NULL);
    }

  fix_char_size(hSize, vSize, 1, 5);

  asf = 1;
  set_arrow_scale_factor(&asf);
  set_clipping(0, 0, 0);
  orig_linetype = get_linetype();
  legendNumber = -1;
  for (i = 0; i < n; i++)
    {
      if (!makeLegend[i])
        continue;
      legendNumber++;
      x[0] = xmin + lpmin * (xmax - xmin) + legend_xrange / 2;
      y[0] = ymin + lqmax * (ymax - ymin) - (3 * legendNumber + 1) * vSize;
      if (((long)strlen(legend[i]) + 1) > bufferSize)
        buffer = trealloc(buffer, sizeof(*buffer) * (bufferSize = strlen(legend[i]) + 1));
      strcpy_ss(buffer, legend[i]);
      set_linethickness(legendThickness);
      cplot_string(x, y, buffer);
      y[0] = y[1] = ymin + lqmax * (ymax - ymin) - (3 * legendNumber + 2) * vSize;
      switch (graphic[i]->element)
        {
        case PLOT_LINE:
        case PLOT_IMPULSE:
        case PLOT_YIMPULSE:
        case PLOT_BAR:
        case PLOT_YBAR:
          x[0] = xmin + (xmax - xmin) * lpmin;
          x[1] = xmin + (xmax - xmin) * lpmax;
          plot_lines(x, y, 2, graphic[i]->type, graphic[i]->thickness);
          break;
        case PLOT_DOTS:
          x[0] = xmin + (xmax - xmin) * lpmin + legend_xrange / 2;
          plot_dots(x, y, 1, graphic[i]->type, ((legendFlags[i] & LEGEND_NOSUBTYPE ? 0 : graphic[i]->subtype)+1)*5);
          break;
        case PLOT_SYMBOL:
          x[0] = xmin + (xmax - xmin) * lpmin + legend_xrange / 2;
          plot_points_fill(x, y, 1, graphic[i]->type, legendFlags[i] & LEGEND_NOSUBTYPE ? 0 : graphic[i]->subtype,
                           graphic[i]->scale * (pmax - pmin) / 0.75, graphic[i]->thickness, graphic[i]->fill);
          if (graphic[i]->flags & GRAPHIC_CONNECT)
            {
              x[0] = xmin + (xmax - xmin) * lpmin;
              x[1] = xmin + (xmax - xmin) * lpmax;
              plot_lines(x, y, 2, graphic[i]->connect_linetype, graphic[i]->thickness);
            }
          set_linetype(orig_linetype);
          break;
        case PLOT_ERRORBAR:
          if (!(legendFlags[i] & LEGEND_NOSUBTYPE) && graphic[i]->subtype != orig_linetype)
            set_linetype(graphic[i]->subtype);
          x[0] = xmin + (xmax - xmin) * lpmin + legend_xrange / 2;
          sx[0] = legend_xrange / 16;
          sy[0] = (legend_yrange / n_legends) / 24;
          plot_error_bars(x, y, sx, sy, 1, graphic[i]->type, graphic[i]->thickness);
          if (!(legendFlags[i] & LEGEND_NOSUBTYPE) && graphic[i]->subtype != orig_linetype)
            set_linetype(orig_linetype);
          break;
        case PLOT_ARROW:
          x[0] = xmin + (xmax - xmin) * lpmin;
          x[1] = xmin + (xmax - xmin) * lpmax;
          if (graphic[i]->arrow.barbLength <= 0)
            plot_lines(x, y, 2, graphic[i]->arrow.linetype, 0);
          else
            plot_arrow(x[0], y[0], x[1] - x[0], 0.0, -1.0, 30. / 180. * PI, graphic[i]->arrow.linetype, graphic[i]->arrow.flags, graphic[i]->arrow.thickness);
          break;
        default:
          bomb("unknown plot code in make_legend()", NULL);
          break;
        }
    }

  set_char_size(h_save, v_save, 1);
  set_arrow_scale_factor(&asf);
  set_clipping(1, 1, 1);
  set_linetype(orig_linetype);
}

long countNonblankStrings(char **string, long strings)
{
  long i, n;
  if (!string)
    return 0;
  for (i = n = 0; i < strings; i++)
    if (string[i] && !SDDS_StringIsBlank(string[i]))
      n++;
  return n;
}

long argEcho_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  argEcho = 1;
  return 1;
}

long multicommand_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  multicommandMode = 1;
  if (repeatMode)
    SDDS_Bomb("-repeat and -multicommand are incompatible");
  return 1;
}

long repeatMode_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  unsigned long flags;
  repeatMode = 1;
  if (multicommandMode)
    SDDS_Bomb("-repeat and -multicommand are incompatible");
  if (!scanItemList(&flags, item, &items, 0,
                    "checkseconds", SDDS_LONG, &repeatModeCheckInterval, 1, 0,
                    "timeout", SDDS_LONG, &repeatModeTimeout, 1, 0,
                    NULL) ||
      repeatModeCheckInterval <= 0 ||
      repeatModeTimeout <= 0)
    {
      SDDS_Bomb("invalid -repeat syntax");
    }
  return 1;
}

void freeN(void *ptr)
{
  if (*((void **)ptr))
    free(*((void **)ptr));
  *((void **)ptr) = NULL;
}

void EraseDatasetSpecification(PLOT_DATA *dataset)
{
  long i;
  if (!dataset->replicant)
    {
      freeN(&dataset->x);
      freeN(&dataset->y);
      freeN(&dataset->x1);
      freeN(&dataset->y1);
      freeN(&dataset->split_data);
      /*freeN(&dataset->legend);*/
      if (dataset->enumerate)
        {
          SDDS_FreeStringArray(dataset->enumerate, dataset->points);
          free(dataset->enumerate);
          dataset->enumerate = NULL;
        }
      if (dataset->pointLabel)
        {
          SDDS_FreeStringArray(dataset->pointLabel, dataset->points);
          free(dataset->pointLabel);
          dataset->pointLabel = NULL;
        }
    }

  for (i = 0; i < 2; i++)
    {
      freeN(&dataset->scalesGroupID[i]);
      /*There is a problem with these free commands
        freeN(&dataset->info[i].symbol);
        freeN(&dataset->info[i].units);*/
      dataset->info[i].symbol = NULL;
      dataset->info[i].units = NULL;
      /*freeN(&dataset->info[i].description);*/
    }
  for (i = 0; i < 4; i++)
    {
      /*freeN(dataset->label+i);*/
      dataset->label[i] = NULL;
    }
  for (i = 0; i < dataset->string_labels; i++)
    {
      freeN(&(dataset->string_label[i].string));
      freeN(&(dataset->string_label[i].edit_command));
      freeN(&(dataset->string_label[i].justify_mode));
    }
  /*freeN(&dataset->string_label);*/
  dataset->string_label = NULL;
  for (i = 0; i < dataset->drawLineSpecs; i++)
    {
      freeN(&(dataset->drawLineSpec[i].x0Param));
      freeN(&(dataset->drawLineSpec[i].y0Param));
      freeN(&(dataset->drawLineSpec[i].p0Param));
      freeN(&(dataset->drawLineSpec[i].q0Param));
      freeN(&(dataset->drawLineSpec[i].x1Param));
      freeN(&(dataset->drawLineSpec[i].y1Param));
      freeN(&(dataset->drawLineSpec[i].p1Param));
      freeN(&(dataset->drawLineSpec[i].q1Param));
    }
  /*freeN(&dataset->drawLineSpec);*/
  dataset->drawLineSpec = NULL;
}

void EraseLabelSpecification(LABEL_SPEC *label)
{
  if (label->flags)
    freeN(&label->label);
  if (label->flags & LABEL_EDITCOMMAND_GIVEN)
    freeN(&label->edit_command);
}

void ErasePanelSpecification(PLOT_PANEL *panel, long *scalesGroups)
{
  freeN(panel->title + 0);
  freeN(panel->title + 1);
  freeN(&panel->dataset);
}

void ErasePlotRequest(PLOT_REQUEST *plreq)
{
  long i;
  for (i = 0; i < plreq->filenames; i++)
    {
      if (plreq->description_text)
        freeN(&plreq->description_text[i]);
      freeN(&plreq->filename[i]);
    }
  freeN(&plreq->filename);
  freeN(&plreq->description_text);
  for (i = 0; i < plreq->datanames; i++)
    {
      freeN(plreq->xname + i);
      freeN(plreq->yname + i);
      freeN(plreq->x1name + i);
      freeN(plreq->y1name + i);
    }
  freeN(&plreq->xname);
  freeN(&plreq->yname);
  freeN(&plreq->x1name);
  freeN(&plreq->y1name);
  freeN(&plreq->was_wildname);
  for (i = 0; i < 2; i++)
    freeN(&(plreq->scalesGroupSpec[i].ID));
  /*
    for (i=0; i<4; i++) {
    freeN(&plreq->label[i].label);
    freeN(&plreq->label[i].edit_command);
    }
    freeN(&(plreq->legend.value));
    freeN(&(plreq->legend.edit_command));
    for (i=0; i<plreq->string_labels; i++) {
    freeN(&plreq->string_label[i].string);
    freeN(&plreq->string_label[i].edit_command);
    freeN(&plreq->string_label[i].justify_mode);
    }
    freeN(&plreq->string_label);
    plreq->string_labels = 0;
    for (i=0; i<plreq->drawLineSpecs; i++) {
    freeN(&(plreq->drawLineSpec[i].x0Param));
    freeN(&(plreq->drawLineSpec[i].y0Param));
    freeN(&(plreq->drawLineSpec[i].p0Param));
    freeN(&(plreq->drawLineSpec[i].q0Param));
    freeN(&(plreq->drawLineSpec[i].x1Param));
    freeN(&(plreq->drawLineSpec[i].y1Param));
    freeN(&(plreq->drawLineSpec[i].p1Param));
    freeN(&(plreq->drawLineSpec[i].q1Param));
    }
    plreq->drawLineSpecs = 0;
    freeN(&plreq->split.name);
    freeN(&plreq->split.ref_string);
    for (i=0; i<plreq->filters; i++) {
    for (j=0; j<plreq->filter[i]->filter_terms; j++) {
    freeN(&plreq->filter[i]->filter_term[j].name);
    freeN(&plreq->filter[i]->filter_term[j]);
    }
    freeN(&plreq->filter[i]->filter_term);
    }
    freeN(&plreq->filter);
    for (i=0; i<plreq->matches; i++) {
    for (j=0; j<plreq->match[i]->match_terms; j++) {
    freeN(&plreq->match[i]->match_term[j].name);
    freeN(&plreq->match[i]->match_term[j].string);
    freeN(&plreq->match[i]->match_term[j]);
    }
    freeN(&plreq->match[i]->match_term);
    }
    freeN(&plreq->match);
    if (plreq->enumerate_settings.flags) {
    SDDS_FreeStringArray(plreq->enumerate_settings.enumerate, plreq->enumerate_settings.enumerates);
    freeN(&plreq->enumerate_settings.enumerateIndex);
    freeN(&plreq->enumerate_settings.editcommand);
    }
    if (plreq->pointLabelSettings.flags) {
    freeN(&plreq->pointLabelSettings.name);
    freeN(&plreq->pointLabelSettings.editCommand);
    freeN(&plreq->pointLabelSettings.justifyMode);
    }
  */
}

void ErasePlotSpecification(PLOT_SPEC *plspec)
{
  PLOT_DATA *dataset;
  long i;

  for (i = 0; i < plspec->plot_requests; i++)
    ErasePlotRequest(plspec->plot_request + i);

  for (i = 0; i < plspec->datasets; i++)
    {
      dataset = plspec->dataset + i;
      EraseDatasetSpecification(dataset);
    }
  freeN(&plspec->dataset);
  plspec->datasets = 0;

  for (i = 0; i < plspec->panels; i++)
    ErasePanelSpecification(plspec->panel + i, plspec->scalesGroups);
  freeN(&plspec->panel);
  plspec->panels = 0;

  plspec->layout[0] = plspec->layout[1] = 0;
  plspec->maxPanelsPerPage = 0;
  freeN(plspec->scalesGroupData + 0);
  freeN(plspec->scalesGroupData + 1);
  freeN(&plspec->plot_request);

  SDDS_ZeroMemory((void *)plspec, sizeof(*plspec));
  add_plot_request(plspec);
}

#define CL_BUFFER_SIZE 16384
int count_arguments(char *s);

long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc)
{
  long i, l;
  static char buffer[CL_BUFFER_SIZE];

  if (!fgets(buffer, CL_BUFFER_SIZE, stdin) ||
      SDDS_StringIsBlank(buffer))
    return 0;
  buffer[strlen(buffer) - 1] = 0;

  *argc = countArguments(buffer) + commandlineArgc;
  *argv = tmalloc((*argc + 1) * sizeof(**argv));
  for (i = 0; i < commandlineArgc; i++)
    SDDS_CopyString((*argv) + i, commandlineArgv[i]);

  while (((*argv)[i] = get_token_t(buffer, " ")))
    {
      l = strlen((*argv)[i]);
      if ((*argv)[i][0] == '"' && (*argv)[i][l - 1] == '"')
        {
          strslide((*argv)[i], -1);
          (*argv)[i][l - 2] = 0;
        }
      i++;
    }

  if (i != *argc)
    SDDS_Bomb("argument count problem in ReadMoreArguments");

  return 1;
}

int countArguments(char *s)
{
  int count;
  char *ptr, *copy;

  count = 0;
  cp_str(&copy, s);

  while ((ptr = get_token_t(copy, " ")))
    {
      count++;
      free(ptr);
    }
  free(copy);
  return (count);
}

void EraseScannedArgs(SCANNED_ARG *scanned, long argc)
{
  long i, j;
  for (i = 0; i < argc; i++)
    {
      for (j = 0; j < scanned[i].n_items; j++)
        freeN(scanned[i].list + j);
      freeN(&scanned[i].list);
    }
}
