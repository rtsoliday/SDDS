/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* i the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: scales()
 * purpose: draw scales for either x or y, with ticks/graticle lin/log
 *          and other options.
 *
 * Michael Borland, 1991.
 $Log: not supported by cvs2svn $
 Revision 1.42  2011/03/06 23:16:46  borland
 Fixed bug in x log scales when -subticks=xNoLogLabel is given (wasn't adjusting
 position of main labels).  Also adjusted the log label position to prevent it
 getting too close to the x axis label.

 Revision 1.41  2011/01/11 22:59:40  soliday
 Removed debugging statement

 Revision 1.40  2011/01/11 22:51:02  soliday
 Changed all the strcpy commands to strcpy_ss because of problems with
 RedHat Enterprise 6. If a strcpy copies the result to the same memory
 space you will get unexpected results.

 Revision 1.39  2010/09/02 15:49:28  soliday
 Removed last change.

 Revision 1.38  2010/09/01 01:08:24  borland
 Changed the order of some calls to fix a bug that showed up when using linetype
 tables and thickness controls.

 Revision 1.37  2005/08/29 21:31:25  shang
 added linetype to xlabel, ylabel, topline and title options

 Revision 1.36  2005/04/26 22:20:14  shang
 added labelScale argument to make_scale_with_label()

 Revision 1.35  2005/03/07 22:45:58  shang
 changed the lower limit label plot scale from 1e-7 to 1e-300

 Revision 1.34  2005/03/03 17:08:48  soliday
 Fixed a possible problem with doExpon not being initialized.

 Revision 1.33  2005/02/22 21:30:44  shang
 put the vertical justification of time-scale plot back

 Revision 1.32  2005/02/22 19:02:36  shang
 replace 'b' by 't' justify mode for ploting time scales

 Revision 1.31  2005/02/22 17:48:18  shang
 added feature of plotting labels of sub-tick for log-scale axis.

 Revision 1.30  2003/09/02 19:29:28  soliday
 Cleaned up code for Linux.

 Revision 1.29  2002/08/14 17:24:51  soliday
 Added Open License

 Revision 1.28  2001/08/13 20:11:51  soliday
 Added thickness options for several options.

 Revision 1.27  2001/06/04 20:03:08  soliday
 Made some changes that only affect sddscontour.

 Revision 1.26  2000/06/02 20:56:36  soliday
 Added thickness option to lineTypeDefault, drawLine, axes, tick and subtick
 options.

 Revision 1.25  2000/04/27 19:28:32  soliday
 Added support for line thickness.

 Revision 1.24  1999/07/21 18:06:30  borland
 Now uses less than 6 ticks (instead of 7) for time scales, to keep labels
 from being too small.

 Revision 1.23  1999/06/25 16:55:44  borland
 Improved placement and scaling of tick label factors (which appear when
 -ticks=xfactor=<value>,yfactor=<value> are used.

 Revision 1.22  1999/06/14 14:22:00  borland
 Tweaked factors for making time scale labels to prevent tick labels and
 axis label from colliding.

 Revision 1.21  1999/06/03 16:07:14  soliday
 Removed compiler warnings under linux.

 Revision 1.20  1999/05/24 18:17:24  borland
 Now support the -ticksettings=[xy]scalechars qualifier for time mode ticks.

 Revision 1.19  1999/05/24 17:51:28  borland
 Fixed some label scaling bugs and reinstated the -labelsize option.

 Revision 1.18  1999/05/18 20:35:22  borland
 Now shows scale factor on labels as the reciprocal, which is more intuitive.

 Revision 1.17  1999/05/03 15:51:14  borland
 Fixed bugs with subticks: no linetype control, no grid control.

 Revision 1.16  1999/02/03 19:38:48  borland
 Improved operation of -range option when the center/range is undefined.
 Improved operation of time-scales when the time range is undefined.

 Revision 1.15  1999/01/04 18:01:04  borland
 Made some changes in character size adjustment for labels when the
 string is very short or has certain characters.

 Revision 1.14  1998/12/22 16:02:36  borland
 New character scaling algorithm for all labels.  Considerable changes
 to psymbol routine that draws characters and returns the size of
 strings.

 Revision 1.13  1998/10/01 22:48:04  borland
 Reimplemented scale and offset qualifiers for -xlabel and -ylabel.
 Implemented {xy}scaleChars qualifier for -ticksettings.
 These options allow users to control the size of characters on scale labels
 and ticks labels.

 Revision 1.12  1998/09/02 14:09:02  borland
 Added -alignZero option.  Also added delay qualifier to -zoom option.
 Changed implementation of panel scale determination.  Also modified
 arguments to make_scale() to permit inverted scales.

 Revision 1.11  1998/08/25 15:05:27  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.10  1998/06/27 14:16:02  borland
 Fixed bug that prevented generation of proper ticks and labels when the
 time range was about 3 years.

 Revision 1.9  1998/06/19 19:51:49  borland
 Expanded range of function for time scales down to smaller values.
 Now works down to ~5ms range on calendar time values.

 Revision 1.8  1996/11/11 21:31:54  borland
 Added control over the type of labels for logscales: -tickSettings option
 now takes [xy]nonExponentialLabels options.

 * Revision 1.7  1996/03/17  04:59:51  borland
 * Added subticks to time scales.
 *
 * Revision 1.6  1996/02/28  21:01:14  borland
 * Added more time scale plotting intervals; fixed problem with tick that appeared
 * off scale.
 *
 * Revision 1.5  1996/02/20  16:29:45  borland
 * Added routines for GIF support using gd1.2 library.  Improved operation
 * of make_time_scales() (now does sub-second ticks).
 *
 * Revision 1.4  1996/02/09  14:47:56  borland
 * Fixed problems with time labels for short time scales; will also use a large
 * number of labels (up to 30) if that is necessary, instead of bombing.
 *
 * Revision 1.3  1996/02/09  00:07:25  borland
 * Added xtime and ytime qualifiers to -ticksettings option, along with code
 * to do time-labeled axes (assumes time values are times in seconds since
 * epoch).
 *
 * Revision 1.2  1995/09/05  21:09:25  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "graphics.h"
#include "SDDS.h"
#include <ctype.h>

#define MAX2(x, y) ((x)>(y)?(x):(y))

void make_scales(
                 int plane,            /* 0/1 for x/y */
                 int do_log,           /* 0=no, 1=yes, exponential labels, 2=yes, explicit labels */
                 int do_labels,        /* logical */
                 double label_modulus, 
                 double tick_fraction, /* if <0, do lines.  if >0, gives fractional tick
                                          size.  if ==0, tick size is chosen by routine */
                 double subtick_fraction,
                 double spacing,       /* tick spacing.  If <=0, will be chosen by routine */
                 double factor,        /* factor to multiply label numbers by */
                 int n_subticks,
                 int linetype,
                 int linethickness,
                 int sublinetype,
                 int sublinethickness
                 )
{
  make_scales_with_label(plane, do_log, do_labels, label_modulus,
                         tick_fraction, subtick_fraction, spacing, factor, n_subticks, 
                         linetype, linethickness, 0, sublinetype, sublinethickness, NULL, 0, 0, 1);
}

void make_scales_with_label(
                            int plane,            /* 0/1 for x/y */
                            int do_log,           /* 0=no, 1=yes, exponential labels, 2=yes, explicit labels */
                            int do_labels,        /* logical */
                            double label_modulus, 
                            double tick_fraction, /* if <0, do lines.  if >0, gives fractional tick
                                                     size.  if ==0, tick size is chosen by routine */
                            double subtick_fraction,
                            double spacing,       /* tick spacing.  If <=0, will be chosen by routine */
                            double factor,        /* factor to multiply label numbers by */
                            int n_subticks,
                            int linetype,
                            int linethickness,
                            long tickLabelThickness,
                            int sublinetype,
                            int sublinethickness,
                            char *label,
                            long labelLineType,
                            long labelThickness,
                            double labelScale
                            )
{
  double xl, xh, yl, yh;
  double pmin, pmax, qmin, qmax;
  double wpmin, wpmax, wqmin, wqmax;
  get_mapping(&xl, &xh, &yl, &yh);
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  get_wspace(&wpmin, &wpmax, &wqmin, &wqmax);
  make_scale(plane, do_log, do_labels, tick_fraction, spacing, linetype, linethickness, tickLabelThickness,
             factor, 0, 0, 0.0, label_modulus, 1.0, n_subticks, 0.0, sublinetype, sublinethickness,
             0, plane?yl:xl, plane?yh:xh, plane?xl:yl,
             plane?(pmin-wpmin)*(xh-xl)/(pmax-pmin):0.67*(qmin-wqmin)*(yh-yl)/(qmax-qmin),
             0, label, labelScale, 0, labelLineType, labelThickness, 1);

  make_scale(plane, do_log, 0, tick_fraction, spacing, linetype, linethickness, tickLabelThickness,
             factor, 0, 0, 0.0, label_modulus, 1.0, n_subticks, 0.0, sublinetype, sublinethickness,
             1, plane?yl:xl, plane?yh:xh, plane?xh:yh,
             plane?(wpmax-pmax)*(xh-xl)/(pmax-pmin):(wqmax-qmax)*(yh-yl)/(qmax-qmin),
	     0, NULL, 1, 0, 0, 0, 1);

}

void find_tick_interval(double *start, double *delta, int *number,
                        int *do_exponential, double lower, double upper)
{
  double mantissa;
  int expon, n_ticks, i;
#define N_INTERVALS 4
  double interval[N_INTERVALS] = {.2, .5, 1.0, 2.0} ;

  if (lower>upper)
    SWAP_DOUBLE(lower, upper);
  /* determine whether to use exponential notation or not */
  if (FABS(upper)>FABS(lower))
    expon = floor(log10(FABS(upper)));
  else
    expon = floor(log10(FABS(lower)));
  
  if (expon>3 || expon<-2) {
    n_ticks = 7;         /* use no more than 7 ticks */
    *do_exponential = 1;
  }
  else {
    n_ticks = 9;         /* use no more than 9 ticks */
    *do_exponential = 0;
  }
  if ((upper+lower)!=0 && log10(fabs((upper+lower)/(2*(upper-lower))))>2.25) {
    if (n_ticks>8)
      n_ticks = 7;
  }
  if (upper<0 || (lower<0 && fabs(upper)<fabs(lower)))
    n_ticks--;

  if (!*delta) {
    mantissa = pow(10.0, log10(upper-lower)-floor(log10(upper-lower)));
    for (i=0; i<N_INTERVALS; i++) {
      if (mantissa/interval[i]<n_ticks)
        break;
    }
    if (i==N_INTERVALS)
      i--;
    *delta = interval[i]*pow(10.0, (double)floor(log10(upper-lower)));
  }
  
  *start = (floor(lower/ *delta)+1) * *delta;
  *number = (upper - *start)/ *delta + 1;
}

void find_format(char *format, double xl, double xh, double dx, int do_exponential)
{
  int nl, nh, nd, n_digits;

  if (xl>xh)
    SWAP_DOUBLE(xl, xh);
  
  nl = (xl?floor(log10(FABS(xl))):0);
  nh = (xh?floor(log10(FABS(xh))):0);
  if (xl==0)
    nl = nh;
  if (xh==0)
    nh = nl;
  nd = (dx?floor(log10(FABS(dx))):0);

  if (do_exponential) {
    n_digits = MAX2(nl, nh) - nd +
      (( fabs( fabs(dx/pow(10.0, 1.0*nd)) -2.5) )<1e-8?1:0);
    sprintf(format, "%%.%dlf$d$ux$v$i10$a%%d$n", n_digits);
  }
  else {
    sprintf(format, "%%.%dlf", 
            nd>=0?0: -nd+((fabs( fabs(dx/pow(10.0, 1.0*nd)) -2.5) )<1e-8?1:0));
  }
}

void make_ticks(
                char dimension,                /* 'x' or 'y' */
                double start,                  /* position of first tick */
                double spacing,                /* spacing of ticks */
                int number,                    /* number of ticks */
                double tick_base,              /* position of base of tick */
                double tick_height,            /* height of the tick */
                int pen,
                int thickness
                )
{
  double xline[2], yline[2];
  
  if (dimension=='y') {
    int iy;
    xline[0] = tick_base;
    xline[1] = tick_base + tick_height;
    for (iy=0; iy<number; iy++) {
      yline[0] = yline[1] = start + iy*spacing;
      plot_lines(xline, yline, 2, pen,thickness);
    }
  }
  else {
    int ix;
    yline[0] = tick_base;
    yline[1] = tick_base + tick_height;
    for (ix=0; ix<number; ix++) {
      xline[0] = xline[1] = start + ix*spacing;
      plot_lines(xline, yline, 2, pen,thickness);
    }
  }
}


void make_subticks(
                   char dimension,                /* 'x' or 'y' */
                   double lower, double upper,    /* lower and upper limits of graph */
                   double spacing,                /* spacing of ticks */
                   double tick_base,              /* position of base of tick */
                   double tick_height,            /* height of the tick */
                   int pen,
		   int thickness
                   )
{
  double start;
  int number;
  start = (floor(lower/spacing)+1)*spacing;
  number = (upper-start)/spacing+1;
  make_ticks(dimension, start, spacing, number, tick_base, tick_height, pen, thickness);
}


/* the following is a more general routine than make_scales(), and should not
 * be confused with it
 */

void make_scale
  (
   int plane,            /* 0/1 for x/y */
   int doLog,           /* logical */
   int doLabels,        /* logical */
   double tickFraction, /* if <0, do lines.  if >0, gives fractional tick
                            size.  if ==0, tick size is chosen by routine */
   double spacing,       /* tick spacing.  If <=0, will be chosen by routine */
   int linetype,         /* linetype for major ticks/lines */
   int linethickness,    /* linethickness for major ticks/lines */
   long tickLabelThickness,    
   double factor,        /* factor to multiply label numbers by */
   long hideFactor,      /* whether to show the factor as part of the scale */
   long invertScale,     /* whether to invert the scale (factor->-factor with hiden sign on factor label) */
   double offset,        /* offset added to label numbers */
   double modulus,       /* modulus applied to label numbers */
   double tickLabelScale, /* scaling of tick label size */
   int nSubticks,       /* number of subticks between major ticks */
   double subtickFraction,
   int sublinetype,      /* linetype for minor ticks/lines */
   int sublinethickness, /* linethickness for minor ticks/lines */
   int side,             /* 0 or 1.  if doing x, {0,1}->{bottom,top}.  
                            if doing y, {0,1}->{left, right} */
   double rangeMin,      /* minimum and maximum values for the scale */
   double rangeMax,      /* in user's coordinates */
   double position,      /* user-space position of line, a y (x) value if doing x (y) scale */
   double allowedSpace,  /* allowed user-space for scale, ticks, and label a dy (dx) value if doing x (y) */
   long adjustForTicks,  /* asks for adjustment of position and allowedSpace to make room for ticks */
   char *labelString,    /* axis label */
   double labelScale,    /* scale factor for label characters */
   double labelOffset,    /* offset (in 0,1 coords for label characters) */
   long labelLineType,   /* color of axis label */
   long labelThickness,
   int no_log_label  /*if 1, do not plot subtick label for log-scale */
   )
{
  /* these are flags/data derived from the argument list */
  long doLeft, doBelow;

  double xl, xh, yl, yh, lowCoord, highCoord;
  double hsize=0, vsize, hsave, vsave;
  double xRange, yRange, x=0, y;
  double xLine[4], xLineShort[4], yLine[4], yLineShort[4];
  double xStart=0, yStart=0, dx, dy;
  int nx, ny, doExpon=0, i, j=0;
  long power, nOrders, sublabels=1;
  double value, tickHeight, subTickBaseFraction;
  char format[256], label[256];
  typedef struct {
    double x, y;
    char label[256];
  } TICK_LABEL;
  TICK_LABEL *tickLabel=NULL;
  double plottedLength, maxPlottedLength;
  double pmin, pmax, qmin, qmax, spaceFactor;
  long delta = 0;

  get_pspace(&pmin, &pmax, &qmin, &qmax);
  get_char_size(&hsave, &vsave, 1);

  if (tickFraction<0 || tickFraction>1)
    tickFraction = plane?0.0125:0.02;
  if (tickFraction==1) 
    subTickBaseFraction = plane?0.0125:0.02;
  else
    subTickBaseFraction = tickFraction;
  if (subtickFraction<=0 || subtickFraction>1)
    subtickFraction = subTickBaseFraction/2;
  
  if (spacing<0)
    spacing = 0;
  if (factor!=1 && doLog) {
    SDDS_Bomb("can't have factor!=1 and do logarithmic scales");
  }
  if (invertScale)
    factor = -factor;
  
  /* only one of these will actually get used */
  doLeft = doBelow = !side;

  widen_window(1);

  get_mapping(&xl, &xh, &yl, &yh);
  if ((xRange = xh-xl)<=0 || (yRange = yh-yl)<=0) {
    SDDS_Bomb("x or y range is <= 0");
    exit(1);
  }

  lowCoord = plane?yl:xl;
  highCoord = plane?yh:xh;
  if (rangeMin==rangeMax) {
    rangeMin = lowCoord;
    rangeMax = highCoord;
  }
  if (rangeMin<lowCoord || rangeMin>highCoord)
    rangeMin = lowCoord;
  if (rangeMax<lowCoord || rangeMax>highCoord)
    rangeMax = highCoord;
  if (rangeMin>=rangeMax) {
    rangeMin = lowCoord;
    rangeMax = highCoord;
  }

  if (adjustForTicks) {
    adjustPositionAndSpaceForTicks(&position, &allowedSpace, 
                                   tickFraction, plane, side);
  }
  xStart = yStart = position;
  if (labelString) {
    long orig_linetype;
    double xpos, ypos;
    orig_linetype=get_linetype();
    if (plane) {
      /* allot 65% of the space to the ticks, plus 25% empty space */
      adjustLabelCharSize(labelString, allowedSpace*(1-0.65)*(1-0.25), plane);
      char_scale(labelScale, labelScale, 1 /*ylabel*/);
      xpos = position + (doLeft?-1:1)*(0.65+0.35/2)*allowedSpace + labelOffset*(xh-xl);
      ypos = (yl+yh)/2;
      vertical_print(doLeft?1:-1);
    }
    else {
      /* alloted 50% of the space to the ticks with 20% empty space */
      adjustLabelCharSize(labelString, allowedSpace*(1-0.5)*(1-0.2), plane);
      char_scale(labelScale, labelScale, 2 /*xlabel*/);
      xpos = (rangeMin+rangeMax)/2;
      ypos = labelOffset*(yh-yl) + 
        (doBelow ?
         position - allowedSpace*(0.5+0.5*0.5) :
         position + allowedSpace*(0.5+0.5*0.5) ) ;
    }
    if (labelLineType>0)
      set_linetype(labelLineType);
    set_linethickness(labelThickness);
    jxyplot_string(&xpos, &ypos, labelString, 'c', 'c');
    if (labelLineType>0)
      set_linetype(orig_linetype);
    if (plane)
      vertical_print(0);
    set_char_size(hsave, vsave, 1);
  }

  if (plane==0) {
    /* scale for x */
    
    get_char_size(&hsize, &vsize, 1);

    /* draw horizontal bar connecting ticks */
    xLine[0] = rangeMin;
    xLine[1] = rangeMax;
    yLine[1] = yLine[0] = yStart;
    plot_lines(xLine, yLine, 2, linetype, linethickness);

    /* calculate y endpoints of ticks */
    yLine[0] = yStart;
    yLine[1] = yStart + (tickHeight=(doBelow?yRange*tickFraction:-yRange*tickFraction));
    yLineShort[0] = yStart;
    yLineShort[1] = yStart + (tickHeight=(doBelow?yRange*tickFraction:-yRange*tickFraction)/2);
    if (!doLog) {
      /* do linear grid in x */

      /* draw ticks */
      dx = spacing;
      find_tick_interval(&xStart, &dx, &nx, &doExpon, 
                         factor*rangeMin+offset, factor*rangeMax+offset);
      xStart = (xStart-offset)/factor;
      dx      /= factor;
      if (nSubticks)
        make_subticks('x', rangeMin, rangeMax, dx/nSubticks, yStart, 
                      (doBelow?1:-1)*yRange*subtickFraction,
                      sublinetype, sublinethickness);
      make_ticks('x', xStart, dx, nx, yStart, tickHeight, linetype, linethickness);

      if (doLabels) {
        /* compose numeric labels */
        find_format(format, xl*factor+offset, xh*factor+offset, dx*factor, doExpon);
        if (!(tickLabel=malloc(sizeof(*tickLabel)*nx)))
          SDDS_Bomb("Memory allocation failure (make_scale)");
	set_linethickness(tickLabelThickness);
        for (i=0; i<nx; i++) {
          tickLabel[i].label[0] = 0;
          tickLabel[i].x = xStart+i*dx;
          tickLabel[i].y = 0;  /* not used */
          makeTickLabel(tickLabel[i].label, format, tickLabel[i].x, 
                        factor, offset, modulus, xRange, doExpon, doLog, 0);
        }
      }
    }
    else {
      /* make logarithmic scale in x*/

      /* draw ticks */
      power = floor(rangeMin);
      nOrders = rangeMax-rangeMin+2.5;
      delta = 1;
      if (nOrders>100) {
        delta = nOrders/100;
        nOrders = 100;
      }
      for (i=0; i<nOrders; i+=delta){
        xStart = dx = pow(10.0, (i+power)*1.0);
	for (j=0; j<10; j++, xStart+=dx) {
	  if ((xLine[0] = xLine[1] = log10(xStart))>rangeMin &&
	      xLine[0]<rangeMax) {
	    plot_lines(xLine, j==0?yLine:yLineShort, 2, linetype,linethickness);
	  }
	  if (nOrders>20) {
	    sublabels = 0;
	    break;
	  }
	  if (nOrders>10 || delta>2) {
	    sublabels = 0;
	  }
	}
      }

      if (doLabels) {
        /* compose numeric labels */
	if ((delta = (long)spacing)<1)
	  delta = 1;
        nx = (rangeMax-rangeMin)/delta + 2.5;
        if (!(tickLabel=malloc(sizeof(*tickLabel)*(nx))))
          SDDS_Bomb("Memory allocation failure (make_scale)");
	set_linethickness(tickLabelThickness);
        for (i=0; i<nx; i++) {
          xStart = dx = pow(10.0, (delta*i+power)*1.0);
          tickLabel[i].label[0] = 0;
          if ((value=log10(xStart))>rangeMin && value<rangeMax) {
            tickLabel[i].y = 0; /* not used */
            tickLabel[i].x = value;
            makeTickLabel(tickLabel[i].label, NULL, xStart, 0, 0, 0, xRange, doExpon, doLog, i+power);
          }
        }
      }
    }
    if (doLabels) {
      maxPlottedLength = 0;
      for (i=0; i<nx; i++)
        if (tickLabel[i].label[0] &&
            (plottedLength=computePlottedStringLength(tickLabel[i].label, 0))>maxPlottedLength)
          maxPlottedLength = plottedLength;
      /* The factor of 0.2 here is just to keep the label characters from getting obnoxiously
         large.  The factor is allowed to be larger as the vertical pspace shrinks. 
         It limits to 50% with 20% empty space, i.e., 40% 
         */
      if ((spaceFactor = 0.2/(qmax-qmin))>0.4)
        spaceFactor = 0.4;
      adjustTickCharSize(0, maxPlottedLength, NULL, xRange/nx, nx, allowedSpace*spaceFactor*(doExpon?0.9:1), plane);
      char_scale(tickLabelScale, tickLabelScale, 3 /*xticks*/);
      get_char_size(&hsize, &vsize, 1);
      if (!no_log_label && doLog)
        y = yStart + (doBelow?-0.375:0.375)*allowedSpace;
      else
        y = yStart + (doBelow?-0.25:0.25)*allowedSpace;
      for (i=0; i<nx; i++) {
        tickLabel[i].y = y;
        if (tickLabel[i].label[0]) {
          jxyplot_string(&tickLabel[i].x, &tickLabel[i].y, tickLabel[i].label, 'c', 'c');
        }
      }
      if (doLog && !no_log_label && delta==1 && sublabels) {
        adjustTickCharSize(0, maxPlottedLength, NULL, xRange/nx/7, nx/7, allowedSpace*spaceFactor, plane);
        char_scale(tickLabelScale, tickLabelScale, 5 /*mini xticks*/);
        get_char_size(&hsize, &vsize, 1);
        y = yStart + (doBelow?-0.12:0.12)*allowedSpace;
        label[0]=0;
        for (i=0;i<nx;i++) {
          if (!i) {
            if (rangeMin<0)
              xStart=(int)rangeMin - 1;
            else
              xStart=(int)rangeMin;
          }
          else
            xStart ++;
          for (j=2; j<10; j++) {
            if ((rangeMax-rangeMin)>2.2 && j%2) 
                continue;
            x =log10(j*1.0)+xStart;
            sprintf(label,"%d",j);
            if (x>=rangeMin && x<=rangeMax) {
              jxyplot_string(&x, &y, label, 'c', 'c'); 
            }
          }
        }
      }
      free(tickLabel);
      if (!doLog && factor!=1 && !hideFactor && !(invertScale && factor==-1)) {
        sprintf(label, "*%g", 1./factor*(invertScale?-1:1));
        plotStringInBox(label, 
                        xh + (xh-xl)*0.10, position-allowedSpace/4,
                        (xh-xl)*0.10, allowedSpace*0.4, 0, 1);
      }
    }
  }
  else {
    /* scale for y */

    /* make a vertical bar */
    yLine[0] = rangeMin;
    yLine[1] = rangeMax;
    xLine[1] = xLine[0] = xStart;
    plot_lines(xLine, yLine, 2, linetype, linethickness); 
    
    /* calculate x values for ends of ticks */
    xLine[0] = xStart;
    xLine[1] = xStart + (tickHeight = (!doLeft?-1:1)*xRange*tickFraction);
    xLineShort[0] = xStart;
    xLineShort[1] = xStart + (tickHeight = (!doLeft?-1:1)*xRange*tickFraction/2);

    if (!doLog) {
      /* do linear grid in y */

      /* make the ticks */
      dy = spacing;
      find_tick_interval(&yStart, &dy, &ny, &doExpon, factor*rangeMin+offset, factor*rangeMax+offset);
      yStart = (yStart-offset)/factor;
      dy      /= factor;
      if (nSubticks) 
        make_subticks('y', yl, yh, dy/nSubticks, xStart, 
                      (!doLeft?-1:1)*xRange*subtickFraction,
                      sublinetype, sublinethickness);
      make_ticks('y', yStart, dy, ny, xStart, tickHeight, linetype, linethickness);

      if (doLabels) {
        /* compose numeric labels */
        find_format(format, yl*factor+offset, yh*factor+offset, dy*factor, doExpon);
        if (!(tickLabel=malloc(sizeof(*tickLabel)*ny)))
          SDDS_Bomb("Memory allocation failure (make_scale)");
	set_linethickness(tickLabelThickness);
        for (i=0; i<ny; i++) {
          tickLabel[i].y = yStart+i*dy;
          tickLabel[i].x = 0;  /* not used */
          makeTickLabel(tickLabel[i].label, format, tickLabel[i].y, 
                        factor, offset, modulus, yRange, doExpon, doLog, 0);
        }
      }
    }
    else {
      /* do logarithmic scale in y */
      /* draw the ticks */
      power = floor(rangeMin);
      nOrders = rangeMax-rangeMin+2.5;
      delta = 1;
      if (nOrders>100) {
        delta = nOrders/100;
        nOrders = 100;
      }
      x = xStart + (!doLeft?1:-1)*hsize*0.5;
      for (i=0; i<nOrders; i++) {
        yStart = dy = pow(10.0, (i*delta+power)*1.0);
	for (j=0; j<10; j++, yStart+=dy) {
	  if ((yLine[0] = yLine[1] = log10(yStart))>rangeMin &&
	      yLine[0]<rangeMax) {
	    plot_lines(j==0?xLine:xLineShort, yLine, 2, linetype, linethickness);
	  }
	  if (nOrders>20) {
	    sublabels = 0;
	    break;
	  }
	  if (nOrders>10 || delta>2) {
	    sublabels = 0;
	  }
	}
      }

      if (doLabels) {
        /* compose numeric labels */
	if ((delta = (long)spacing)<1)
	  delta = 1;
	ny = (rangeMax-rangeMin)/delta + 2.5;
        if (!(tickLabel=malloc(sizeof(*tickLabel)*ny)))
          SDDS_Bomb("Memory allocation failure (make_scale)");
	set_linethickness(tickLabelThickness);
        for (i=0; i<ny; i++) {
          yStart = pow(10.0, (i*delta+power)*1.0);
          tickLabel[i].label[0] = 0;
          if ((value=log10(yStart))>rangeMin && value<rangeMax) {
            tickLabel[i].x = 0;  /* not used */
            tickLabel[i].y = value;
            makeTickLabel(tickLabel[i].label, NULL, yStart, 0, 0, 0, yRange, doExpon, doLog, i+power);
          }
        }
      }
    }
    if (doLabels) {
      maxPlottedLength = 0;
      for (i=0; i<ny; i++)
        if (tickLabel[i].label[0] &&
            (plottedLength=computePlottedStringLength(tickLabel[i].label, 0))>maxPlottedLength)
          maxPlottedLength = plottedLength;
      adjustTickCharSize(0, maxPlottedLength, NULL, yRange/ny, ny, allowedSpace*0.65*(1-0.25), plane);
      char_scale(tickLabelScale, tickLabelScale, 4 /*yticks*/);
      get_char_size(&hsize, &vsize, 1);
      x = xStart + (!doLeft?1:-1)*hsize*0.5;
      for (i=0; i<ny; i++) {
        tickLabel[i].x = x;
        if (tickLabel[i].label[0]) {
          jxyplot_string(&tickLabel[i].x, &tickLabel[i].y, tickLabel[i].label, !doLeft?'l':'r', !doLog?'b':'c'); 
        }
      }
      if (doLog && !no_log_label && delta==1 && sublabels) {
        adjustTickCharSize(0, maxPlottedLength, NULL, yRange/ny/8, ny*8, allowedSpace*0.65*(1-0.25), plane);
        char_scale(tickLabelScale, tickLabelScale, 5 /*mini yticks*/);
        get_char_size(&hsize, &vsize, 1);
        x = xStart + (!doLeft?1:-1)*hsize*0.7;
        label[0]=0;
        for (i=0;i<ny;i++) {
          if (!i) {
            if (rangeMin<0)
              yStart=(int)rangeMin - 1;
            else
              yStart=(int)rangeMin;
          }
          else
            yStart ++;
          for (j=2; j<10; j++) {
            if ((rangeMax-rangeMin)>2.2 && j%2)
              continue;
            y =log10(j*1.0)+yStart;
            sprintf(label,"%d",j);
            if (y>=rangeMin && y<=rangeMax) {
              jxyplot_string(&x, &y, label, 'c', 'c'); 
            }
          }
        }
      }
      free(tickLabel);
      if (!doLog && factor!=1 && !hideFactor && !(invertScale && factor==-1)) {
        sprintf(label, "*%g", 1./factor*(invertScale?-1:1));
        plotStringInBox(label, 
                        position-(doLeft?1:-1)*allowedSpace/2, yh + (yh-yl)*0.05,
                        allowedSpace*0.9, (yh-yl)*0.03, 0, 1);
      }
    }
  }
  
  set_char_size(hsave, vsave, 1);
  widen_window(0);
}

#include <time.h>

void make_time_scales(int plane,
                      double tickFraction,
                      int linetype)
{
  double xl, xh, yl, yh;
  get_mapping(&xl, &xh, &yl, &yh);
  makeTimeScales(plane, tickFraction, linetype,
                 xl, xh, NULL, 0, 0.0, 0, 1.0, 1.0, 1.0, 1.0, 1.0);
}


/* Convenient structure and routines for handling position of time ticks */
struct TIME_TICK {
  double epoch;
  double sec;
  short min, hour, day, month, year;
};

void convertEpochToBreakdown(struct TIME_TICK *tp) 
{
  double hour;
  TimeEpochToBreakdown(&(tp->year), NULL, &(tp->month), &(tp->day), &hour, tp->epoch);
  tp->hour = hour;
  tp->min = 60*(hour-tp->hour);
  tp->sec = 3600*hour - 60*(tp->hour*60+tp->min);
}

void convertBreakdownToEpoch(struct TIME_TICK *tp) 
{
  TimeBreakdownToEpoch(tp->year, 0, tp->month, tp->day, tp->hour + tp->min/60.0 + tp->sec/3600.0, &(tp->epoch));
}  

struct TIME_SPACING {
  double sec;
  int min, hour, day, month, year;
  int subdivisions;
  double spacing;
} timeSpacing[36] = {
{ 0.001, 0, 0, 0, 0, 0, 5, 0.0},
{ 0.002, 0, 0, 0, 0, 0, 4 , 0.0},
{ 0.005, 0, 0, 0, 0, 0, 5 , 0.0},
{ 0.01, 0, 0, 0, 0, 0, 5 , 0.0},
{ 0.02, 0, 0, 0, 0, 0, 4 , 0.0},
{ 0.05, 0, 0, 0, 0, 0, 5 , 0.0},
{ 0.1, 0, 0, 0, 0, 0, 5 , 0.0},
{ 0.2, 0, 0, 0, 0, 0, 4 , 0.0},
{ 0.5, 0, 0, 0, 0, 0, 5, 0.0},
{ 1, 0, 0, 0, 0, 0, 5, 0.0},
{ 2, 0, 0, 0, 0, 0, 4, 0.0},
{ 5, 0, 0, 0, 0, 0, 5, 0.0},
{ 10, 0, 0, 0, 0, 0, 5 , 0.0},
{ 15, 0, 0, 0, 0, 0, 3 , 0.0},
{ 20, 0, 0, 0, 0, 0, 4, 0.0},
{ 30, 0, 0, 0, 0, 0, 6 , 0.0},
{ 60, 0, 0, 0, 0, 0, 6 , 0.0},
{ 0, 5, 0, 0, 0, 0, 5, 0.0},  
{ 0, 15, 0, 0, 0, 0, 3, 0.0},  
{ 0, 30, 0, 0, 0, 0, 6, 0.0},  
{ 0, 60, 0, 0, 0, 0, 6, 0.0},  
{ 0, 0, 2, 0, 0, 0, 4, 0.0},  
{ 0, 0, 4, 0, 0, 0, 4, 0.0},  
{ 0, 0, 6, 0, 0, 0, 6, 0.0},  
{ 0, 0, 12, 0, 0, 0, 6, 0.0},  
{ 0, 0, 24, 0, 0, 0, 6, 0.0},  
{ 0, 0, 0, 2, 0, 0, 4, 0.0},  
{ 0, 0, 0, 5, 0, 0, 5, 0.0},  
{ 0, 0, 0, 10, 0, 0, 10, 0.0},  
{ 0, 0, 0, 15, 0, 0, 3, 0.0},  
{ 0, 0, 0, 30, 0, 0, 6, 0.0},  
{ 0, 0, 0, 0, 2, 0, 4, 0.0},  
{ 0, 0, 0, 0, 3, 0, 3, 0.0},  
{ 0, 0, 0, 0, 4, 0, 4, 0.0},  
{ 0, 0, 0, 0, 6, 0, 6, 0.0},  
{ 0, 0, 0, 0, 12, 0, 6, 0.0},  
};

void regularizeTickPosition(struct TIME_TICK *tp, int i)
{
  if (i>=0 && i<36) {
    int delta;
    if (tp->sec>=60) {
      delta = tp->sec/60;
      tp->min += delta;
      tp->sec -= 60*delta;
    }
    if (tp->min>=60) {
      delta = tp->min/60;
      tp->hour += delta;
      tp->min -= 60*delta;
    }
    if (tp->hour>=24) {
      delta = tp->hour/24;
      tp->day += delta;
      tp->hour -= 24*delta;
    }
    if (timeSpacing[i].min)
      tp->min -= tp->min%timeSpacing[i].min;
    if (timeSpacing[i].hour)
      tp->hour -= tp->hour%timeSpacing[i].hour;
    if (timeSpacing[i].day)
      tp->day -= tp->day%timeSpacing[i].day;
    if (timeSpacing[i].month)
      tp->month -= tp->month%timeSpacing[i].month;
    convertBreakdownToEpoch(tp);
  }  
}

void incrementTickPosition(struct TIME_TICK *tp, double multiplier, int i)
{
  short delta;
  if (i>=0 && i<36) {
    convertBreakdownToEpoch(tp);
/*    fprintf(stderr,"** Incrementing time position ds=%.3g, dmin=%d, dh=%d, dd=%d, dm=%d, dy=%d\n",
            timeSpacing[i].sec, timeSpacing[i].min, timeSpacing[i].hour, timeSpacing[i].day, timeSpacing[i].month, timeSpacing[i].year);
    fprintf(stderr, "Start: epoch=%12.10f, s=%.10f, min=%d, hour=%d, day=%d, month=%d, year=%d\n",
            tp->epoch, tp->sec, tp->min, tp->hour, tp->day, tp->month, tp->year);
*/
    tp->min += multiplier*timeSpacing[i].min;
    tp->hour += multiplier*timeSpacing[i].hour;
    tp->day += multiplier*timeSpacing[i].day;
    tp->month += multiplier*timeSpacing[i].month;
    tp->year += multiplier*timeSpacing[i].year;
    tp->sec += multiplier*timeSpacing[i].sec;
    if (tp->sec>=60) {
      delta = tp->sec/60;
      tp->min += delta;
      tp->sec -= 60*delta;
    }
    if (tp->min>=60) {
      delta = tp->min/60;
      tp->hour += delta;
      tp->min -= 60*delta;
    }
    if (tp->hour>=24) {
      delta = tp->hour/24;
      tp->day += delta;
      tp->hour -= 24*delta;
    }
/*    fprintf(stderr, "Incr: s=%.10f, min=%d, hour=%d, day=%d, month=%d, year=%d\n",
            tp->sec, tp->min, tp->hour, tp->day, tp->month, tp->year);
*/
    convertBreakdownToEpoch(tp);
    convertEpochToBreakdown(tp);
    if (tp->sec>=60) {
      delta = tp->sec/60;
      tp->min += delta;
      tp->sec -= 60*delta;
    }
    if (tp->min>=60) {
      delta = tp->min/60;
      tp->hour += delta;
      tp->min -= 60*delta;
    }
    if (tp->hour>=24) {
      delta = tp->hour/24;
      tp->day += delta;
      tp->hour -= 24*delta;
    }
/*    fprintf(stderr, "End: epoch=%12.10f, s=%.10f, min=%d, hour=%d, day=%d, month=%d, year=%d\n",
            tp->epoch, tp->sec, tp->min, tp->hour, tp->day, tp->month, tp->year);
*/
  }
}


void makeTimeScales(
                    int plane,            /* 0/1 for x/y */
                    double tickFraction, /* if <0, do lines.  if >0, gives fractional tick
                                             size.  if ==0, tick size is chosen by routine */
                    int linetype,
                    double position1,     /* position of main, labeled scale */
                    double position2,     /* position of secondary scale.  If ==position1, not done. */
                    char *scaleLabel,     /* label for the scale */
                    long scaleLabelLineType, /* color for scale label */
                    double allowedSpace,   /* size in user coordinates allowed for this scale and label */
                    long adjustForTicks,   /* adjust position1 and allowedSpace to make room for ticks */
                    double labelCharScale, /* user's requested increase in character sizes */
                    double tickCharScale,
                    int linethickness,
                    int tickLabelThickness,
                    int labelThickness
                    )
{
  /* flags and values derived from the input values */
  long doY, doBothScales;
  double xl, xh, yl, yh;
  double xRange, yRange, x, y;
  double xLine[4], yLine[4], xsLine[4], ysLine[4];
  double timeStart, timeRange, timeEnd, subtickPosition, tickStart;
  long i, iUse, doTicks, first, firstSubticks;
  char label[256];
  double hsave, vsave, spacing, subspacing;
  double hsize, vsize;
  double qmin, qmax, pmin, pmax;
  double wqmin, wqmax, wpmin, wpmax;
  long labelMode, ticks, triesMade, subTicks, subTick;
  char labelSample[20];
  struct TIME_TICK tickPosition, lastTickPosition;
  short extraIncrement;

#define HSECOND 0.5
#define TSECOND 0.1
#define SECOND 1.0
#define MINUTE (60*SECOND)
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define MONTH (30*DAY)
#define YEAR (365*DAY)
  
#define LABEL_MINUTESECOND 1
#define LABEL_HOURMINUTE 2
#define LABEL_DAYHOUR 3
#define LABEL_MONTHDAY 4
#define LABEL_YEARMONTH 5
#define LABEL_YEAR 6
#define LABEL_SECOND 7
#define LABEL_TSECOND 8
#define LABEL_HSECOND 9

  for (i=0; i<36; i++) {
    /* compute spacing of ticks in seconds */
    timeSpacing[i].spacing = 
      timeSpacing[i].sec + 60*(timeSpacing[i].min + 60*(timeSpacing[i].hour + 24*(timeSpacing[i].day + 30.4375*(timeSpacing[i].month + 365.25*timeSpacing[i].year))));
  }
  
  get_char_size(&hsave, &vsave, 1);
  
  doY = plane;
  doBothScales = position1==position2?0:1;
  doTicks = (tickFraction>=0);

  if ((tickFraction = fabs(tickFraction))==0)
    tickFraction = doY?0.015:0.02;
  if (!doTicks && position1==position2) {
    /* can't do lines if only one scale position is given */
    doTicks = 1;
    tickFraction = doY?0.015:0.02;
  }
  if (adjustForTicks) {
    adjustPositionAndSpaceForTicks(&position1, &allowedSpace, 
                                   tickFraction, plane, 0);
    if (!doBothScales)
      position2 = position1;
  }
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  get_wspace(&wpmin, &wpmax, &wqmin, &wqmax);
  
  widen_window(1);

  get_mapping(&xl, &xh, &yl, &yh);
  if ((xRange = xh-xl)<=0)
    SDDS_Bomb("x range is <= 0");
  if ((yRange = yh-yl)<=0)
    SDDS_Bomb("y range is <= 0");

  if (!allowedSpace)
    allowedSpace = doY ? 
      (pmin-wpmin)*xRange/(pmax-pmin) :
        (qmin-wqmin)*yRange/(qmax-qmin) ;

  if (doY) {
    timeStart = yl;
    timeRange = yRange;
    timeEnd = yh;
  }
  else {
    timeStart = xl;
    timeRange = xRange;
    timeEnd = xh;
  }
  
  if (scaleLabel) {
    double xpos, ypos;
    long orig_linetype=0;
    if (doY) {
      adjustLabelCharSize(scaleLabel, allowedSpace/3.5, doY);
      xpos = position1 - allowedSpace*0.75;
      ypos = (yl+yh)/2;
      vertical_print(1);
    }
    else {
      adjustLabelCharSize(scaleLabel, allowedSpace/2.5, doY);
      xpos = (xl+xh)/2;
      ypos = position1-allowedSpace*0.75;
    }
    get_char_size(&hsize, &vsize, 1);
    hsize *= labelCharScale;
    vsize *= labelCharScale;
    if (doY) {
      fix_char_size(hsize, vsize, 1, 1);
    } else {
      fix_char_size(hsize, vsize, 1, 2);
    }
    if (scaleLabelLineType>0)
      orig_linetype=set_linetype(scaleLabelLineType);
    set_linethickness(labelThickness);
    jxyplot_string(&xpos, &ypos, scaleLabel, 'c', 'c');
    if (doY) 
      vertical_print(0);
    set_char_size(hsave, vsave, 1);
    if (scaleLabelLineType>0)
      set_linetype(orig_linetype);
  }

  if (doY) {
    /* draw long lines for scales */
    xLine[0] = xLine[1] = position1;
    yLine[0] = yl;
    yLine[1] = yh;
    plot_lines(xLine, yLine, 2, linetype, linethickness);
    if (doBothScales) {
      xLine[0] = xLine[1] = position2;
      plot_lines(xLine, yLine, 2, linetype, linethickness);
    }
  }
  else {
    yLine[0] = yLine[1] = position1;
    xLine[0] = xl;
    xLine[1] = xh;
    plot_lines(xLine, yLine, 2, linetype, linethickness);
    if (doBothScales) {
      yLine[0] = yLine[1] = position2;
      plot_lines(xLine, yLine, 2, linetype, linethickness);
    }
  }
  
   
  i = iUse = 0;
  while (i<36) {
    ticks = (long)(timeRange/timeSpacing[i].spacing);
    if (ticks>3 && ticks<6) {
      iUse = i;
      break;
    }
    if (ticks>=3 && ticks<30)
      iUse = i;
    i++;
  }
  if (iUse==-1) {
    /* probably no data, but can't tell.  Just return without making scales */
    return;
  }
  ticks = (long)(timeRange/timeSpacing[iUse].spacing);
  spacing = timeSpacing[iUse].spacing;
  subspacing = spacing/timeSpacing[iUse].subdivisions;
  subTicks = timeSpacing[iUse].subdivisions - 1;

  if (spacing<=HSECOND) {
    tickStart = (long)timeStart +
      ((long)((timeStart-(long)timeStart)*100))/100.0;
    if (timeRange<HSECOND)
      tickStart += ((long)(1000*(timeStart-tickStart)))/1000.0;
    labelMode = LABEL_HSECOND;
    strcpy_ss(labelSample, "9.999s");
  }
  else if (spacing<=TSECOND) {
    tickStart = (long)timeStart +
      ((long)((timeStart-(long)timeStart)*10))/10.0;
    if (timeRange<TSECOND) {
      tickStart += ((long)(100*(timeStart-tickStart)))/100.0;
      labelMode = LABEL_HSECOND;
      strcpy_ss(labelSample, "9.999s");
    } else {
      labelMode = LABEL_TSECOND;
      strcpy_ss(labelSample, "9.99s");
    }
  }
  else if (spacing<=SECOND) {
    tickStart = (long)timeStart;
    tickStart += ((long)(10*(timeStart-tickStart)))/10.0;
    labelMode = LABEL_SECOND;
    strcpy_ss(labelSample, "9.9s");
  }
  else if (spacing<MINUTE) {
    tickStart = (long)(timeStart+0.5);
    labelMode = LABEL_MINUTESECOND;
    strcpy_ss(labelSample, "60m60s");
  }
  else if (spacing<60*MINUTE) {
    tickStart = ((long)(timeStart/MINUTE+0.5))*60;
    labelMode = LABEL_HOURMINUTE;
    strcpy_ss(labelSample, "24h60m");
  }
  else if (spacing<24*HOUR) {
    tickStart = ((long)(timeStart/HOUR+0.5))*HOUR;
    labelMode = LABEL_DAYHOUR;
    strcpy_ss(labelSample, "30d24h");
  }
  else if (spacing<30*DAY) {
    tickStart = ((long)(timeStart/DAY+0.5))*DAY;
    labelMode = LABEL_MONTHDAY;
    strcpy_ss(labelSample, "12/30");
  }
  else if (spacing<11*MONTH) {
    tickStart = ((long)(timeStart/MONTH+0.5))*MONTH;
    labelMode = LABEL_YEARMONTH;
    strcpy_ss(labelSample, "12/1998");
  }
  else {
    tickStart = ((long)(timeStart/YEAR+0.5))*YEAR;
    labelMode = LABEL_YEAR;
    strcpy_ss(labelSample, "9999");
  }
  
  if (doY) {
    adjustTickCharSize(0, 0.0, labelSample, spacing, ticks, allowedSpace/2.75, 1);
    /* set up arrays for making ticks */
    if (doTicks) {
      xLine[0] = position1;
      xLine[1] = position1+xRange*tickFraction;
      xLine[2] = position2;
      xLine[3] = position2-xRange*tickFraction;

      xsLine[0] = position1;
      xsLine[1] = position1+xRange*tickFraction/2;
      xsLine[2] = position2;
      xsLine[3] = position2-xRange*tickFraction/2;
    }
    else {
      xLine[0] = position1;
      xLine[1] = position2;
    }
  }
  else {
    adjustTickCharSize(0, 0.0, labelSample, spacing, ticks, allowedSpace/3.25, 0);
    if (doTicks) {
      yLine[0] = position1;
      yLine[1] = position1+yRange*tickFraction;
      yLine[2] = position2;
      yLine[3] = position2-yRange*tickFraction;

      ysLine[0] = position1;
      ysLine[1] = position1+yRange*tickFraction/2;
      ysLine[2] = position2;
      ysLine[3] = position2-yRange*tickFraction/2;
    }
    else {
      yLine[0] = position1;
      yLine[1] = position2;
    }
  }

  if (tickCharScale>0) {
    get_char_size(&hsize, &vsize, 1);
    hsize *= tickCharScale;
    vsize *= tickCharScale;
    if (doY) {
      fix_char_size(hsize, vsize, 1, 4);
    } else {
      fix_char_size(hsize, vsize, 1, 3);
    }
  }
  
  tickPosition.epoch = tickStart - spacing; 
  convertEpochToBreakdown(&tickPosition);
  regularizeTickPosition(&tickPosition, iUse);
  lastTickPosition = tickPosition;
  first = 1;
  firstSubticks = 1;
  triesMade = 0;
  extraIncrement = 0;
  while ( (((labelMode == LABEL_YEARMONTH) && (tickPosition.epoch - MONTH<timeEnd)) || 
           ((labelMode != LABEL_YEARMONTH) && (tickPosition.epoch<timeEnd))) && 
          triesMade<4*ticks) {
    triesMade++;
    switch (labelMode) {
    case LABEL_HSECOND:
      sprintf(label, "%.3fs", tickPosition.sec);
      break;
    case LABEL_TSECOND:
      sprintf(label, "%.2fs", tickPosition.sec);
      break;
    case LABEL_SECOND:
      sprintf(label, "%.1fs", tickPosition.sec);
      break;
    case LABEL_MINUTESECOND:
      sprintf(label, "%dm%02ds", tickPosition.min, (int)tickPosition.sec);
      break;
    case LABEL_HOURMINUTE:
      if (tickPosition.sec>30)
        tickPosition.min += 1;
      tickPosition.sec = 0;
      if (first)
        tickPosition.min = (tickPosition.min/((long)timeSpacing[iUse].spacing/MINUTE))*((long)timeSpacing[iUse].spacing/MINUTE);
      sprintf(label, "%dh%02dm", tickPosition.hour, tickPosition.min);
      break;
    case LABEL_DAYHOUR:
      tickPosition.sec = 0;
      if (tickPosition.min>30)
        tickPosition.hour += 1;
      tickPosition.min = 0;
      if (first)
        tickPosition.hour = (tickPosition.hour/((long)timeSpacing[iUse].spacing/HOUR))*((long)timeSpacing[iUse].spacing/HOUR);
      sprintf(label, "%dd%dh", tickPosition.day, tickPosition.hour);
      break;
    case LABEL_MONTHDAY:
      tickPosition.sec = tickPosition.min = tickPosition.hour = 0;
      sprintf(label, "%d/%d", tickPosition.month, tickPosition.day);
      break;
    case LABEL_YEARMONTH:
      tickPosition.sec = tickPosition.min = tickPosition.hour = 0;
      tickPosition.day = 1;
      sprintf(label, "%d/%d", tickPosition.year, tickPosition.month);
      break;
    case LABEL_YEAR:
      tickPosition.sec = tickPosition.min = tickPosition.hour = 0;
      tickPosition.day = tickPosition.month = 1;
      sprintf(label, "%d",  tickPosition.year);
      break;
    }
    convertBreakdownToEpoch(&tickPosition);
    if (spacing>SECOND) {
      if (tickPosition.epoch<timeStart) {
        extraIncrement += 1;
        tickPosition = lastTickPosition;
        incrementTickPosition(&tickPosition, extraIncrement, iUse);
        continue;
      } 
      else if (!first && (tickPosition.epoch-lastTickPosition.epoch)<spacing/2) {
/*        fprintf(stderr, "last epoch = %.2f, epoch = %.2f, delta = %.2f\n",
                lastTickPosition.epoch, tickPosition.epoch, tickPosition.epoch-lastTickPosition.epoch);
*/
        extraIncrement += 1;
        tickPosition = lastTickPosition;
        incrementTickPosition(&tickPosition, extraIncrement, iUse);
        continue;
      }
    } else {
      if (tickPosition.epoch<timeStart) {
        tickPosition = lastTickPosition;
        extraIncrement += 1;
        incrementTickPosition(&tickPosition, extraIncrement, iUse);
        continue;
      }
    }
    
    first = 0;
    if (tickPosition.epoch>timeEnd)
      break;
    if (doY) {
      yLine[0] = yLine[1] = yLine[2] = yLine[3] = tickPosition.epoch;
    }
    else {
      xLine[0] = xLine[1] = xLine[2] = xLine[3] = tickPosition.epoch;
    }
    plot_lines(xLine, yLine, 2, linetype,0);
    if (doTicks && doBothScales)
      plot_lines(xLine+2, yLine+2, 2, linetype, linethickness);
    if (tickCharScale>0) {
      if (doY) {
        double xcenter, dummy;
        y = tickPosition.epoch;
        computePlottedStringSize(label, &dummy, &dummy, &xcenter, &dummy, 0);
        x = position1 - 1.1*xcenter/2.0;
        set_linethickness(tickLabelThickness);
        jxyplot_string(&x, &y, label, 'r', 'c');
      }
      else {
        double ylength, dummy;
        x = tickPosition.epoch;
        computePlottedStringSize(label, &dummy, &ylength, &dummy, &dummy, 0);
        y = position1 - ylength*1.1;
        set_linethickness(tickLabelThickness);
        jxyplot_string(&x, &y, label, 'c', 'b');
      }
    }
    
    if (firstSubticks) {
      subTick = -subTicks;
      firstSubticks = 0;
    } else
      subTick = 1;
    for (; subTick<=subTicks; subTick++) {
      subtickPosition = tickPosition.epoch + subTick*subspacing;
      if (subtickPosition<timeStart)
        continue;
      if (subtickPosition>timeEnd)
        break;
      if (doY)
        ysLine[0] = ysLine[1] = ysLine[2] = ysLine[3] = subtickPosition;
      else 
        xsLine[0] = xsLine[1] = xsLine[2] = xsLine[3] = subtickPosition;
      plot_lines(xsLine, ysLine, 2, linetype, linethickness);
      if (doTicks && doBothScales)
        plot_lines(xsLine+2, ysLine+2, 2, linetype, linethickness);
    }

    lastTickPosition = tickPosition;
    incrementTickPosition(&tickPosition, 1, iUse); 
    extraIncrement = 0;
  }
  
  set_char_size(hsave, vsave, 1);
  widen_window(0);
}

void adjustLabelCharSize(char *label, double allowedSpace, long plane)
{
  double hsize, vsize, hExtent, vExtent, xRange, yRange, hFactor, vFactor, factor;
  double xl, xh, yl, yh, dummy;
  int vp = 0, i;

  if (!label || !strlen(label) || allowedSpace==0)
    return;
  get_mapping(&xl, &xh, &yl, &yh);
  if ((xRange = xh-xl)<=0)
    bomb("x range is <= 0", NULL);
  if ((yRange = yh-yl)<=0)
    bomb("y range is <= 0", NULL);
  if (plane) { 
    vp = vertical_print(1);
    computePlottedStringSize(label, &hExtent, &vExtent, &dummy, &dummy, 0);
    /* compare length to size of plot  */
    vFactor = vExtent/(yRange*0.95);
    /* compare the height of the characters to the allowed space */
    hFactor = hExtent/allowedSpace;
  }
  else {
    computePlottedStringSize(label, &hExtent, &vExtent, &dummy, &dummy, 0);
    hFactor = hExtent/(xRange*0.95);
    /* compare the height of the characters to the allowed space */
    vFactor = vExtent/allowedSpace;
  }
  factor = 1;
  if (hFactor<1 || vFactor<1) 
    factor = MAX(hFactor, vFactor);
  if (hFactor>1 || vFactor>1)
    factor = MAX(hFactor, vFactor);

  /* increase the factor if the string is all lower case */
  for (i=0; i<strlen(label); i++)
    if (!islower(label[i]))
      break;
  if (i==strlen(label))
    factor *= 1.5;
  else if (plane && strlen(label)<6 && !strcspn(label, "gjpqy\'\"")) {
    /* reduce size to avoid very large letters */
    /* Commenting out because this resulted in confusion with a -layout=1,2 plot where the yaxes label were different sizes.
       Maybe I should also comment out the case where the size factor is changed for all lower case as well.
       computePlottedStringSize has been changed too */
    //factor *= 1.5;
  }
  
  if (factor!=1) {
    get_char_size(&hsize, &vsize, 0);
    hsize /= factor;
    vsize /= factor; 
    set_char_size(hsize, vsize, 0); 
  }
  if (plane)
    vertical_print(vp);
}

void adjustTickCharSize(long labelLength, double plottedLength, 
                        char *sampleLabel, double spacing, 
                        long ticks, double allowedSpace, long plane)
/* Determine the character size for the tick labels */
{
  double hsize, vsize, factor, vFactor, hFactor;
  get_char_size(&hsize, &vsize, 1);
  if (sampleLabel) {
    computePlottedStringSize(sampleLabel, &plottedLength, &vsize, NULL, NULL, 0);
  } else {
    if (!plottedLength)
      plottedLength = labelLength*hsize;
    if (plottedLength<hsize)
      plottedLength = hsize;
  }
  if (plane) {
    /* vertical */
    /* scale to use allowed space */
    factor = allowedSpace/plottedLength;
    if ((vFactor = (factor*vsize)/(spacing*ticks/20.0))>1)
      /* don't allow characters to be larger than 5% of scale height */
      factor /= vFactor;
    if ((vFactor=(factor*vsize)/(spacing*0.5))>1)
      /* don't allow characters to be larger than 50% of tick spacing */
      factor /= vFactor;
  }
  else {
    /* horizontal */
    /* scale up to use allowed space */
    factor = allowedSpace/vsize; 
    if ((hFactor = (factor*plottedLength)/(spacing*0.7))>1)
      /* scale down to avoid running labels together */
      factor /= hFactor;
  }
  hsize *= factor;
  vsize *= factor;
  set_char_size(hsize, vsize, 1);
}

void makeTickLabel(char *label, char *format, double value, 
                   double factor, double offset, double modulus,
                   double scale, long doExpon, long doLog, long power)
{
  if (doLog) {
    if (value>1e3 || value<1e-3)
      sprintf(label, "10$a%ld$n", power);
    else
      sprintf(label, "%g", value);
  }
  else {
    long exponent;
    double mantissa;
    value = value*factor+offset;
    if (modulus>0) {
      if ((value -= ((long)(value/modulus))*modulus)<0)
        value += modulus;
    }
    if (doExpon) {
      if (FABS(value)<scale/1e300) 
        sprintf(label, "0"); 
      else { 
        mantissa = SIGN(value)*
          pow(10.0, log10(FABS(value))-(exponent=floor(log10(FABS(value)))));
        sprintf(label, format, mantissa, exponent);
     } 
    }
    else
      sprintf(label, format, value);
  }
}  

void adjustPositionAndSpaceForTicks(double *position, double *allowedSpace,
                                    double tickFraction, long plane, long side)
{
  double xl, xh, yl, yh;
  
  get_mapping(&xl, &xh, &yl, &yh);
  if (plane) {
    *position -= tickFraction*(xh-xl)*(side?-1:1);
    *allowedSpace -= tickFraction*(xh-xl);
  }
  else {
    *position -= tickFraction*(yh-yl)*(side?-1:1);
    *allowedSpace -= tickFraction*(yh-yl);
  }
}
