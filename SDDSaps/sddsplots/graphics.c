/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: graphics2.c
 * purpose: graphics utility routines using GNUPLOT drivers.
 *
 * Michael Borland, 1991.
 */

#include <assert.h>
#include <ctype.h>
#include "mdb.h"
#include "graphics.h"
#include "graph.h"

#if defined(_WIN32)
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define DEBUG 0

extern int term;
int screen_ok;
int clip_points;
int clip_lines1;
int clip_lines2;
float xsize = 1;
float ysize = 1;

#define RAD(x) (x * 0.01745329252)

/* Definitions: "plot space" or "pspace"--region in device coordinates that
 *                  data is plotted in.
 *              "user's coordinates"--coordinates defined by values given for
 *                  the corners of the plot space.
 *              "window"--region outside of which all lines are normally
 *                  clipped.  Using identical to the pspace.
 */

/* The boundary of the plot space in device coordinates: */
static int xleft, xright, ybot, ytop;

/* ratio of a pixel's height to its width--needed by psymbol() and to set to 1-to-1 aspect */
static double device_aspect = 1;

/* The boundary of the plot space in (0,1)x(0,1) coordinates */
static int pspace_set = 0;
static double pmin, pmax, qmin, qmax;
static int set_aspect_pending = 0;
static double pending_aspect = 0;

/* The boundary of the 'work space' in (0, 1)x(0, 1) coordinates.
 * If there is one plot per page, it is (0,1)x(0,1)
 */
/*
  static int workSpaceSet = 0;
*/
static double wpmin = 0, wqmin = 0, wpmax = 1, wqmax = 1;

/* The boundary of the legend region in coordinates where the pspace is
 * (0,1)x(0,1)
 */
static double lpmin = 1.02, lpmax = 1.18, lqmin = 0, lqmax = 0.9;

/* The boundary of the clipping window in user's coordinates--set with
 * set_window().  Usually the same as scaling window set with set_mapping(). */
double xw_min, xw_max, yw_min, yw_max;

/* The boundary of the window in device coordinates--used for clipping.
 * Most commonly the same as boundary of plot space. */
static int xw_left, xw_right, yw_bot, yw_top;

/* Boundary and scale factors, in user coordinates */
static int users_coords_set = 0;
static double xmin = 0, xmax = 0, ymin = 0, ymax = 0; /* user's coordinate limits */
static double xrange = 0, yrange = 0;                 /* range of user's coordinates corresponding to plot space */
static double xscale = 0, yscale = 0;                 /* ratio of (xright-left)/xrange etc. */
static double x_abs_max = 0, y_abs_max = 0;           /* user's coordinates with largest 
                                                         absolute values */
/* current pen position, in user's coordinates--set with pmove(xu, yu) */
double xu_pos, yu_pos;
/* stuff having to do with software characters */
static double character_angle = 0;
static double character_tilt = 0;
#define DEFAULT_CHARACTER_SIZE 0.02
/* Character size in units where 1 is width of frame */
static double character_size = DEFAULT_CHARACTER_SIZE;
static double character_aspect = 1.0;
static int vertical_print_direction = 1.0;

/* default line type */
static int mpl_linetype = 0;
static int mpl_force_linetype = 0;
static FILE *terminput = NULL;

typedef struct
{
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

static FONT_SIZE fontsize;

#include "gnumacros.h"
#include "gnugraph.c"

/* Map from terminal to user coordinates */
double unmap_x(double xt)
{
  return (xt - xleft) / xscale + xmin;
}
double unmap_y(double yt)
{
  return (yt - ybot) / yscale + ymin;
}

void graphics_on(void)
{
  struct termentry *t = &term_tbl[term];
  if (!term_init)
    {
      (*t->init)();
      term_init = TRUE;
    }
  screen_ok = FALSE;
  if (!pspace_set)
    {
      if ((t->flags) & TERM_XWINDOWS)
        set_pspace(0.15, 0.9, 0.17, 0.92);
      else if ((t->flags) & TERM_IBMCLONE)
        set_pspace(0.15, 0.9, 0.18, 0.90);
      else if (strcmp(t->name, "sun") == 0)
        set_pspace(0.15, 0.9, 0.17, 0.92);
      else
        set_pspace(0.15, 0.9, 0.16, 0.85);
    }
  if (!terminput)
#if defined(__TURBOC__) || defined(_WIN32)
    terminput = stdin;
#else
  terminput = fopen(ctermid(NULL), "r");
#endif
  (*t->graphics)();
  /* don't use set_linetype() here--want to make sure the driver call gets made */
  (*t->linetype)(mpl_linetype = 0);
}

void graphics_off(void)
{
  struct termentry *t = &term_tbl[term];
  (*t->text)();
  (*t->reset)();
  screen_ok = TRUE;
}

void alloc_spectrum(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1)
{
  struct termentry *t = &term_tbl[term];
  (*t->spectral)(num, spec, red0, green0, blue0, red1, green1, blue1);
}

void frame_end(int hold_screen)
{
  struct termentry *t = &term_tbl[term];
  fflush(outfile);
  if (t->flags & TERM_FLUSHSTDOUT)
    {
      (*t->text)();
      fflush(outfile);
    }
  if (outfile == stdout)
    {
      if (!(t->flags & TERM_NOPROMPT))
        {
          if ((hold_screen & 1) || ((hold_screen & 2) && !(t->flags & TERM_WINDOWS)))
            {
              fputc('\007', stderr);
              if (terminput)
                fgetc(terminput);
              else
                getchar();
            }
          else if (hold_screen & 2)
            {
              while (1)
                sleep(UINT_MAX);
            }
        }
    }
  if (!(t->flags & TERM_WINDOWS))
    graphics_off();
  fflush(outfile);
}

void get_wspace(double *wp_min, double *wp_max, double *wq_min, double *wq_max)
{
  *wp_min = wpmin;
  *wp_max = wpmax;
  *wq_min = wqmin;
  *wq_max = wqmax;
}

void set_wspace(double wp_min, double wp_max, double wq_min, double wq_max)
{
  wpmin = wp_min;
  wpmax = wp_max;
  wqmin = wq_min;
  wqmax = wq_max;
}

void get_pspace(double *p_min, double *p_max, double *q_min, double *q_max)
{
  *p_min = pmin;
  *p_max = pmax;
  *q_min = qmin;
  *q_max = qmax;
}

void set_pspace(double p_mini, double p_maxi, double q_mini, double q_maxi)
{
  struct termentry *t = &term_tbl[term];

  /* Set the values for the edges of the plotting region in device
   * coordinates.  Used by set_mapping() to compute scale factors for 
   * conversion from user's coordinates.   Also used by map_x and
   * map_y macros.
   */
  if (p_maxi - p_mini <= 0.001)
    fprintf(stderr, "possible error: p_mini=%e, p_maxi=%e (set_pspace)\n", p_mini, p_maxi);
  if (q_maxi - q_mini <= 0.001)
    fprintf(stderr, "possible error: q_mini=%e, q_maxi=%e (set_pspace)\n", q_mini, q_maxi);
  if ((pmin = p_mini) < 0)
    pmin = 0;
  if ((pmax = p_maxi) > 1)
    pmax = 1;
  if ((qmin = q_mini) < 0)
    qmin = 0;
  if ((qmax = q_maxi) > 1)
    qmax = 1;
  if ((xleft = (t->xmax - 1) * pmin + 0.5) >= (xright = (t->xmax - 1) * pmax + 0.5))
    {
      fprintf(stderr, "bad pspace definition (set_pspace): [%e, %e] x [%e, %e]\n",
              pmin, pmax, qmin, qmax);
      fprintf(stderr, "t->xmax = %d\n", t->xmax);
      fprintf(stderr, "\7\7please record this printout and email to soliday@anl.gov\n");
      exit(1);
    }
  if ((ybot = (t->ymax - 1) * qmin + 0.5) >= (ytop = (t->ymax - 1) * qmax + 0.5))
    {
      fprintf(stderr, "bad pspace definition (set_pspace): [%e, %e] x [%e, %e]\n",
              pmin, pmax, qmin, qmax);
      fprintf(stderr, "t->ymax = %d\n", t->ymax);
      fprintf(stderr, "\7\7please record this printout and email to soliday@anl.gov\n");
      exit(1);
    }
  if (users_coords_set)
    {
      xscale = (xright - xleft) / xrange;
      yscale = (ytop - ybot) / yrange;
    }
}

void get_legend_space(double *lp_min, double *lp_max, double *lq_min, double *lq_max)
{
  *lp_min = lpmin;
  *lp_max = lpmax;
  *lq_min = lqmin;
  *lq_max = lqmax;
}

void set_legend_space(double lp_min, double lp_max, double lq_min, double lq_max)
{
  if ((lpmin = lp_min) >= (lpmax = lp_max))
    bomb("horizontal legend space is invalid", NULL);
  if ((lqmin = lq_min) >= (lqmax = lq_max))
    bomb("vertical legend space is invalid", NULL);
}

void set_mapping(double xl, double xh, double yl, double yh)
{
  struct termentry *t = &term_tbl[term];
  xrange = (xmax = xh) - (xmin = xl);
  yrange = (ymax = yh) - (ymin = yl);

  if (xrange == 0 || yrange == 0)
    {
      xrange = yrange = xmin = xmax = ymin = ymax = 0;
      users_coords_set = 0;
    }
  else
    {
      users_coords_set = 1;
      if (fabs(x_abs_max = xmax) < fabs(xmin))
        x_abs_max = xmin;
      if (fabs(y_abs_max = ymax) < fabs(ymin))
        y_abs_max = ymin;

      if ((xscale = (xright - xleft) / xrange) <= 0)
        {
          fprintf(stderr, "error: horizontal scale is improperly defined (set_mapping)");
          fprintf(stderr, "x user coordinate range : [%e, %e]\n",
                  xmin, xmax);
          exit(1);
        }
      if ((yscale = (ytop - ybot) / yrange) <= 0)
        {
          fprintf(stderr, "error: vertical scale is improperly defined (set_mapping)");
          fprintf(stderr, "y user coordinate range : [%e, %e]\n",
                  ymin, ymax);
          exit(1);
        }

      /* set default clipping window */
      set_window(xmin, xmax, ymin, ymax);

      if (set_aspect_pending)
        set_aspect(pending_aspect);
    }
  (*t->sendCoordinates)();
}

void get_mapping(
                 double *x_minimum, double *x_maximum,
                 double *y_minimum, double *y_maximum)
{
  *x_minimum = xmin;
  *x_maximum = xmax;
  *y_minimum = ymin;
  *y_maximum = ymax;
}

void set_window(double xlo, double xhi, double ylo, double yhi)
{
  if (!users_coords_set)
    bomb("can't set window if mapping isn't set first", NULL);

  if ((xw_left = map_x(xw_min = xlo)) >= (xw_right = map_x(xw_max = xhi)))
    {
      fprintf(stderr, "plot window improperly set (set_window)\n");
      fprintf(stderr, "input data: xlo=%e, xhi=%e, ylo=%e, yhi=%e\n",
              xlo, xhi, ylo, yhi);
      fprintf(stderr, "mapping range: [%e, %e] x [%e, %e]\n",
              xmin, xmax, ymin, ymax);
      fprintf(stderr, "xscale = %e, xleft = %d, xright = %d\n",
              xscale, xleft, xright);
      fprintf(stderr, "\7\7please record this printout and email to soliday@anl.gov\n");
      exit(1);
    }
  if ((yw_bot = map_y(yw_min = ylo)) >= (yw_top = map_y(yw_max = yhi)))
    {
      fprintf(stderr, "plot window improperly set (set_window)\n");
      fprintf(stderr, "input data: xlo=%e, xhi=%e, ylo=%e, yhi=%e\n",
              xlo, xhi, ylo, yhi);
      fprintf(stderr, "mapping range: [%e, %e] x [%e, %e]\n",
              xmin, xmax, ymin, ymax);
      fprintf(stderr, "yscale = %e, ybot = %d, ytop = %d\n",
              yscale, ybot, ytop);
      fprintf(stderr, "\7\7please record this printout and email to soliday@anl.gov\n");
      exit(1);
    }
}

void set_clipping(int clip_pts, int clip_l1, int clip_l2)
{
  if (clip_pts >= 0)
    clip_points = clip_pts;
  if (clip_l1 >= 0)
    clip_lines1 = clip_l1;
  if (clip_l2 >= 0)
    clip_lines2 = clip_l2;
}

void border(void)
{
  struct termentry *t = &term_tbl[term];

  check_scales("border");
  (*t->move)(xleft, ybot);
  (*t->vector)(xleft, ytop);
  (*t->vector)(xright, ytop);
  (*t->vector)(xright, ybot);
  (*t->vector)(xleft, ybot);
}

void plot_lines_sever(double *xd, double *yd, long n, int line_type, int line_thickness)
{
  int i1, i2, n1;
  double x1, x2;
  check_scales("plot_lines_sever");
  i1 = 0;
  n1 = n - 1;
  line_type = set_linetype(line_type);
  set_linethickness(line_thickness);
  while (i1 < n - 1)
    {
      x1 = xd[i1];
      i2 = i1 + 1;
      while (i2 <= n1 && (x2 = xd[i2]) >= x1)
        {
          x1 = x2;
          i2++;
        }
      plot_lines(xd + i1, yd + i1, i2 - i1, PRESET_LINETYPE, 0);
      i1 = i2;
    }
  set_linetype(line_type);
}

double compute_mean_gap(double *x, long n)
{
  double sum = 0, diff;
  double *x1;
  long i;

  if (n < 2)
    return 0;
  x1 = x;
  for (i = n - 1; i > 0; i--)
    {
      x1++;
      if ((diff = *x1 - *x) < 0)
        sum += -diff;
      else
        sum += diff;
      x = x1;
    }
  return sum / (n - 1);
}

void plot_lines_gap(double *xd, double *yd, double xgap, double ygap, long n, int line_type, int line_thickness)
{
  int i1, i2, n1;
  double x1, x2 = 0;
  double y1, y2 = 0;

  if (!xgap && !ygap)
    return;
  if (xgap < 0)
    xgap *= -compute_mean_gap(xd, n);
  if (ygap < 0)
    ygap *= -compute_mean_gap(yd, n);
  i1 = 0;
  n1 = n - 1;
  line_type = set_linetype(line_type);
  set_linethickness(line_thickness);
  while (i1 < n - 1)
    {
      x1 = xd[i1];
      y1 = yd[i1];
      i2 = i1 + 1;
      while (i2 <= n1 &&
             (!xgap || fabs(x1 - (x2 = xd[i2])) <= xgap) &&
             (!ygap || fabs(y1 - (y2 = yd[i2])) <= ygap))
        {
          x1 = x2;
          y1 = y2;
          i2++;
        }
      plot_lines(xd + i1, yd + i1, i2 - i1, PRESET_LINETYPE, 0);
      i1 = i2;
    }
  set_linetype(line_type);
}

void dplot_lines(int *xd, int *yd, long n, int line_type)
{
  int i; /* point index */
  struct termentry *t = &term_tbl[term];

  check_scales("dplot_lines");
  line_type = set_linetype(line_type);
  (*t->move)(xd[0], yd[0]);
  for (i = 1; i < n; i++)
    (*t->vector)(xd[i], yd[i]);
  set_linetype(line_type);
}

void plot_points_fill(double *xd, double *yd, long n, long point_type, long point_subtype, double scale, int thickness, int fill)
{
  int i;
  int x, y;
  long origLinetype = -1;

  if (point_type < 0)
    bomb("point type less than 0 (plot_points)", NULL);
  if (point_subtype >= 0)
    origLinetype = set_linetype(point_subtype);
  set_linethickness(thickness);
  if (fill == 1)
    set_linethickness(1);
  for (i = 0; i < n; i++)
    {
      x = map_x(xd[i]);
      y = map_y(yd[i]);
      if (!clip_points || (inrange(x, xw_left, xw_right) && inrange(y, yw_bot, yw_top)))
        {
          if (fill == 1)
            {

              do_point_fill(x, y, point_type, scale);
            }
          else
            do_point(x, y, point_type, scale);
        }
    }
  if (origLinetype >= 0)
    set_linetype(origLinetype);
}

void plot_points(double *xd, double *yd, long n, long point_type, long point_subtype, double scale, int thickness)
{
  int i;
  int x, y;
  long origLinetype = -1;

  if (point_type < 0)
    bomb("point type less than 0 (plot_points)", NULL);
  if (point_subtype >= 0)
    origLinetype = set_linetype(point_subtype);
  set_linethickness(thickness);
  for (i = 0; i < n; i++)
    {
      x = map_x(xd[i]);
      y = map_y(yd[i]);
      if (!clip_points || (inrange(x, xw_left, xw_right) && inrange(y, yw_bot, yw_top)))
        {
          do_point(x, y, point_type, scale);
        }
    }
  if (origLinetype >= 0)
    set_linetype(origLinetype);
}

void plot_impulses(double *xd, double *yd, long n, int line_type, int line_thickness)
{
  int i;
  double x[2], y[2];
  double yrange;

  check_scales("plot_impulses");
  yrange = ymax - ymin;
  line_type = set_linetype(line_type);
  for (i = 0; i < n; i++)
    {
      y[0] = 0;
      y[1] = yd[i];
      if (fabs(yd[i]) < yrange / 325.0)
        {
          y[1] = yrange / 750.0;
          y[0] = -y[1];
        }
      x[0] = x[1] = xd[i];
      plot_lines(x, y, 2, PRESET_LINETYPE, line_thickness);
    }
  set_linetype(line_type);
}

void plot_yimpulses(double *xd, double *yd, long n, int line_type, int line_thickness)
{
  int i;
  double x[2], y[2];
  double xrange;

  check_scales("plot_yimpulses");
  xrange = xmax - xmin;
  line_type = set_linetype(line_type);
  for (i = 0; i < n; i++)
    {
      x[0] = 0;
      x[1] = xd[i];
      if (fabs(xd[i]) < xrange / 325.0)
        {
          x[1] = xrange / 750.0;
          x[0] = -x[1];
        }
      y[0] = y[1] = yd[i];
      plot_lines(x, y, 2, PRESET_LINETYPE, line_thickness);
    }
  set_linetype(line_type);
}

void plot_bars(double *xd, double *yd, long n, int line_type, int line_thickness)
{
  int i;
  double x[2], y[2];

  line_type = set_linetype(line_type);
  for (i = 0; i < n; i++)
    {
      y[0] = ymin + line_thickness * yrange / 750;
      y[1] = yd[i];
      x[0] = x[1] = xd[i];
      plot_lines(x, y, 2, PRESET_LINETYPE, line_thickness);
    }
  set_linetype(line_type);
}

void plot_ybars(double *xd, double *yd, long n, int line_type, int line_thickness)
{
  int i;
  double x[2], y[2];

  line_type = set_linetype(line_type);
  for (i = 0; i < n; i++)
    {
      x[0] = xmin;
      x[1] = xd[i];
      y[0] = y[1] = yd[i];
      plot_lines(x, y, 2, PRESET_LINETYPE, line_thickness);
    }
  set_linetype(line_type);
}

void plot_dots(double *xd, double *yd, long n, int dot_type, int dot_subtype)
{
  int i, x, y, offset;
  struct termentry *t = &term_tbl[term];

  check_scales("plot_dots");
  dot_type = set_linetype(dot_type);
  for (i = 0; i < n; i++)
    {
      x = map_x(xd[i]);
      y = map_y(yd[i]);
      if (!clip_points || (inrange(x, xw_left, xw_right) && inrange(y, yw_bot, yw_top)))
        {
          for (offset = 1; offset <= dot_subtype; offset++)
            {
              (*t->move)(x + offset, y + offset);
              (*t->vector)(x - offset, y + offset);
              (*t->vector)(x - offset, y - offset);
              (*t->vector)(x + offset, y - offset);
              (*t->vector)(x + offset, y + offset);
            }
          (*t->dot)(x, y, -1);
        }
    }
  mpl_force_linetype = 1;
  set_linetype(dot_type);
}

void pmove(double x, double y)
{
  struct termentry *t = &term_tbl[term];

  check_scales("pmove");
  xu_pos = x;
  yu_pos = y;
  (*t->move)(map_x(xu_pos), map_y(yu_pos));
}

void get_position(double *x, double *y)
{
  *x = xu_pos;
  *y = yu_pos;
}

void pdraw(double x, double y, int pen_down)
{
  struct termentry *t = &term_tbl[term];

  check_scales("pdraw");
  xu_pos = x;
  yu_pos = y;
  if (pen_down)
    (*t->vector)(map_x(xu_pos), map_y(yu_pos));
  else
    (*t->move)(map_x(xu_pos), map_y(yu_pos));
}

void set_aspect(double aspect)
{
  struct termentry *t = &term_tbl[term];
  double dev_ratio, ratio;

  set_aspect_pending = 0;
  if (!users_coords_set)
    {
      set_aspect_pending = 1;
      pending_aspect = aspect;
      return;
    }

  dev_ratio = ((float)t->ymax) / t->xmax * device_aspect;

  if (aspect > 0)
    {
      /* adjust pspace, leaving xrange and yrange as they are */
      ratio = (pmax - pmin) * yrange * aspect / ((qmax - qmin) * xrange * dev_ratio);
      if (ratio > 1)
        /* must contract x plot-space */
        set_pspace((double)pmin, (double)pmin + (double)(pmax - pmin) / ratio,
                   (double)qmin, (double)(qmax));
      else
        /* must contract y plot-space */
        set_pspace((double)pmin, (double)pmax, (double)qmin,
                   (double)qmin + (double)(qmax - qmin) * ratio);
      set_mapping(xmin, xmax, ymin, ymax);
    }
  else
    {
      /* adjust xrange and yrange, leaving pspace as is */
      ratio = -(qmax - qmin) * dev_ratio / ((pmax - pmin) * aspect);
      if (ratio < yrange / xrange)
        {
          double xcenter;
          /* Must increase xrange. */
          xrange = yrange / ratio;
          xcenter = (xmax + xmin) / 2;
          xmax = xcenter + xrange / 2;
          xmin = xmax - xrange;
          set_mapping((double)xmin, (double)xmax, (double)ymin,
                      (double)ymax);
        }
      else
        {
          double ycenter;
          /* Must increase yrange. */
          yrange = xrange * ratio;
          ycenter = (ymax + ymin) / 2;
          ymax = ycenter + yrange / 2;
          ymin = ymax - yrange;
          set_mapping((double)xmin, (double)xmax, (double)ymin,
                      (double)ymax);
        }
    }
}

void computeAspectAdjustedLimits(double *limitNew, double *limit, double aspect)
{
  struct termentry *t = &term_tbl[term];
  double dev_ratio, ratio, range, center;

  limitNew[0] = limit[0];
  limitNew[1] = limit[1];
  limitNew[2] = limit[2];
  limitNew[3] = limit[3];

  dev_ratio = ((float)t->ymax) / t->xmax * device_aspect;
  ratio = (qmax - qmin) * dev_ratio / ((pmax - pmin) * aspect);
  if (ratio < (limit[3] - limit[2]) / (limit[1] - limit[0]))
    {
      /* Must increase xrange. */
      range = (limit[3] - limit[2]) / ratio;
      center = (limit[1] + limit[0]) / 2;
      limitNew[0] = center - range / 2;
      limitNew[1] = center + range / 2;
    }
  else
    {
      /* Must increase yrange. */
      range = (limit[1] - limit[0]) * ratio;
      center = (limit[3] + limit[2]) / 2;
      limitNew[2] = center - range / 2;
      limitNew[3] = center + range / 2;
    }
}

void plot_string(double *x, double *y, char *s)
{
  static long length, justif;
  static double plotted_length;
  static int xpos, ypos;

#if DEBUG
  fprintf(stderr, "plot_string\n");
#endif
  xu_pos = *x;
  yu_pos = *y;
  xpos = map_x(xu_pos);
  ypos = map_y(yu_pos);

  translate_hershey_codes(s);
  if (!(length = strlen(s)))
    return;
  justif = RETURN_LENGTH;
  plotted_length = psymbol(xpos, ypos, s,
                           (float)(xright - xleft) * character_size,
                           character_aspect, device_aspect, character_angle,
                           character_tilt, length, justif);
  justif = LEFT_JUSTIFY;
  psymbol(xpos, ypos, s,
          (float)(xright - xleft) * character_size,
          character_aspect, device_aspect, character_angle,
          character_tilt, length, justif);
  *x = (xu_pos += plotted_length * cos((double)RAD(character_angle)) / xscale);
  *y = (yu_pos += plotted_length * sin((double)RAD(character_angle)) / yscale);
}

void cplot_string(double *x, double *y, char *s)
/* plot_string centered about x or y--works only for Hershey characters */
/* y is the coordinate of the baseline of the text---NOT the bottom of the
 * plotted string: use jxyplot(x, y, s, 'c', 'b') for the latter. */
{
  static long length, justif;
  static double plotted_length;
  static int xpos, ypos;

  xu_pos = *x;
  yu_pos = *y;
  xpos = map_x(xu_pos);
  ypos = map_y(yu_pos);

  if (!(plotted_length = computePlottedStringLength(s, COMPPLOTTEDSIZE_DEVICEUNITS)) ||
      !(length = strlen(s)))
    return;

  xpos -= plotted_length * cos((double)RAD(character_angle)) / 2;
  ypos -= plotted_length * sin((double)RAD(character_angle)) / 2;

  justif = LEFT_JUSTIFY;
  psymbol(xpos, ypos, s,
          (float)(xright - xleft) * character_size,
          character_aspect, device_aspect, character_angle,
          character_tilt, length, justif);
  *x = (xu_pos += plotted_length / xscale * cos((double)RAD(character_angle)) / 2);
  *y = (yu_pos += plotted_length / yscale * sin((double)RAD(character_angle)) / 2);
}

void jplot_string(double *x, double *y, char *s, char mode)
/* plot string with variable justification along the length of the string */
{
  jxyplot_string(x, y, s, mode, 'b');
  return;
}

void rotate(double *x, double *y, double angle, double xo, double yo);

void jxyplot_string(double *x, double *y, char *s, char xmode, char ymode)
/* plot string with variable justification */
{
  long length, justif;
  int xpos, ypos, padding;
  double x_offset, y_offset;
  double xlength, ylength, xcenter, ycenter;
  char *ptr;

  xu_pos = *x;
  yu_pos = *y;
  xpos = map_x(xu_pos);
  ypos = map_y(yu_pos);

  computePlottedStringSize(s, &xlength, &ylength,
                           &xcenter, &ycenter,
                           COMPPLOTTEDSIZE_NOROTATE + COMPPLOTTEDSIZE_DEVICEUNITS);
  if (!(length = strlen(s)))
    return;
#if DEBUG
  fprintf(stderr, "  size in user coords: %le, %le @ %le, %le\n",
          xlength / xscale, ylength / yscale, xcenter / xscale, ycenter / yscale);
#endif
  if (*(ptr = s + length - 1) == ' ')
    {
      padding = 0;
      while (*ptr == ' ')
        {
          padding++;
          ptr--;
        }
      xlength *= (length + padding) / (1.0 * length);
    }

  switch (xmode)
    {
    case 'r':
      x_offset = -xlength;
      break;
    case 'c':
      x_offset = -xlength / 2;
      break;
    case 'l':
      x_offset = 0;
      break;
    default:
      x_offset = 0;
      break;
    }
  switch (ymode)
    {
    case 't':
      y_offset = -ylength / 2 - ycenter;
      break;
    case 'c':
      y_offset = -ycenter;
      break;
    case 'b':
      y_offset = ylength / 2 - ycenter;
    default:
      y_offset = 0;
      break;
    }
  if (character_angle)
    rotate(&x_offset, &y_offset, character_angle, 0.0, 0.0);

  xpos += x_offset;
  ypos += y_offset;
  justif = LEFT_JUSTIFY;
  psymbol(xpos, ypos, s,
          (float)(xright - xleft) * character_size,
          character_aspect, device_aspect, character_angle,
          character_tilt, length, justif);
}

void plotStringInBox(char *s, double xc, double yc, double dx, double dy,
                     unsigned long mode, short lockSize)
{
  double xlength, ylength, xoffset, yoffset;
  double previousSize, hFactor, vFactor, factor;
  previousSize = character_size;
  if (!lockSize)
    {
      computePlottedStringSize(s, &xlength, &ylength,
                               &xoffset, &yoffset, 0);
      hFactor = dx / xlength;
      vFactor = dy / ylength;
      factor = hFactor;
      if (hFactor < 1 || vFactor < 1)
        factor = MIN(hFactor, vFactor);
      if (hFactor > 1 || vFactor > 1)
        factor = MIN(hFactor, vFactor);
      character_size *= factor;
    }
  jxyplot_string(&xc, &yc, s, 'c', 'c');
  character_size = previousSize;
}

void rotate(double *x, double *y, double angle, double xo, double yo)
{
  double xp, yp;

#if DEBUG
  fprintf(stderr, "Rotating %le, %le by %le about %le, %le\n",
          *x, *y, angle, xo, yo);
#endif
  angle *= PI / 180;
  *x -= xo;
  *y -= yo;

  xp = *x * cos(angle) - *y * sin(angle);
  yp = *x * sin(angle) + *y * cos(angle);

  *x = xp + xo;
  *y = yp + yo;
#if DEBUG
  fprintf(stderr, "   result is %le, %le\n", *x, *y);
#endif
}

/* #define X_LABEL_Y_POSITION (-0.130) */

static double xlabel_offset = 0, ylabel_offset = 0;
static double xlabel_scale = 1, ylabel_scale = 1;

void plot_xlabel(char *label)
{
  long length;
  double plotted_length;
  int justif;
  double saved_character_size;
  double x, y;
  double x_save, y_save;
  int xpos, ypos;

#if DEBUG
  fprintf(stderr, "plot_xlabel\n");
#endif
  translate_hershey_codes(label);
  if ((length = strlen(label)) <= 0)
    return;

  get_position(&x_save, &y_save);
  xpos = map_x(x_save);
  ypos = map_y(y_save);

  saved_character_size = character_size;

  character_size *= xlabel_scale;
  justif = RETURN_LENGTH;
  plotted_length = psymbol(xpos, ypos, label,
                           (float)(xright - xleft) * character_size,
                           character_aspect, device_aspect, character_angle,
                           character_tilt, length, justif) /
    xscale;

  x = (xmin + xmax) / 2;
  y = ymin - 3.5 * character_size * character_aspect * (1.0 * xright - xleft) / (1.0 * ytop - ybot) * yrange;
  if (qmin != qmax)
    {
      y += xlabel_offset * (ymax - ymin) / (qmax - qmin);
      if (((y - ymin) / yrange * (qmax - qmin) + qmin) < 0)
        {
          y = -qmin / (qmax - qmin) * yrange + ymin +
            0.75 * character_size * character_aspect * (1.0 * xright - xleft) / (1.0 * ytop - ybot) * yrange;
        }
    }

  if (plotted_length > xrange)
    character_size /= plotted_length / xrange;

  widen_window(1);
  cplot_string(&x, &y, label);
  widen_window(0);

  character_size = saved_character_size;
  pmove(x_save, y_save);
}

static int title_at_top = 0;

void set_title_at_top(int mode)
{
  title_at_top = mode;
}

/*
  #define TITLE_Y_POSITION (-0.200)
  #define TOPTITLE_Y_POSITION (0.090)
*/

#define OAGSIGN(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

int vertical_print(int turn_on)
{
  static long already_on = 0;
  long previous;
  previous = already_on;
  if (turn_on)
    {
      if (!already_on)
        {
          char_angle(vertical_print_direction * 90.0 * OAGSIGN(turn_on), 0.0);
          already_on = 1;
        }
    }
  else if (already_on)
    {
      char_angle(0.0, 0.0);
      already_on = 0;
    }
  return previous;
}

void set_vertical_print_direction(int direction)
{
  if (direction != 1 && direction != -1)
    direction = 1;
  vertical_print_direction = direction;
}

/*
  #define Y_LABEL_X_POSITION_MINUS (-0.17)
  #define Y_LABEL_X_POSITION_PLUS (-0.16)
*/

void plot_ylabel(char *label)
{
  long length;
  double plotted_length;
  int justif;
  double saved_character_size, x, y, pminChar, x_save, y_save;
  int xpos, ypos;

#if DEBUG
  fprintf(stderr, "plot_ylabel\n");
#endif
  translate_hershey_codes(label);
  if ((length = strlen(label)) <= 0)
    return;

  get_position(&x_save, &y_save);
  xpos = map_x(x_save);
  ypos = map_y(y_save);

  saved_character_size = character_size;
  character_size *= ylabel_scale;

  justif = RETURN_LENGTH;
  plotted_length = psymbol(xpos, ypos, label,
                           (float)(xright - xleft) * character_size,
                           character_aspect, device_aspect, character_angle,
                           character_tilt, length, justif) /
    xscale;

  if (plotted_length > xrange)
    character_size /= plotted_length / xrange;

  plotted_length = psymbol(xpos, ypos, label,
                           (float)(xright - xleft) * character_size,
                           character_aspect, device_aspect, character_angle,
                           character_tilt, length, justif);

  y = (ymin + ymax) / 2 - vertical_print_direction * plotted_length / yscale / 2;
  if (vertical_print_direction > 0)
    {
      x = xmin - 8.5 * xrange * saved_character_size;
      pminChar = xrange * saved_character_size * (pmax - pmin) / xrange;
    }
  else
    {
      x = xmin - 9.5 * xrange * saved_character_size;
      pminChar = 0;
    }

  if (pmax != pmin)
    {
      x += ylabel_offset * (xmax - xmin) / (pmax - pmin);
      if (((x - xmin) / xrange * (pmax - pmin) + pmin) < pminChar)
        {
          x = -pmin / (pmax - pmin) * xrange + xmin + saved_character_size * xrange * 0.5;
          if (vertical_print_direction > 0)
            x += saved_character_size * xrange;
        }
    }

  widen_window(1);
  vertical_print(1);

  plot_string(&x, &y, label);
  vertical_print(0);
  widen_window(0);

  character_size = saved_character_size;
  pmove(x_save, y_save);
}

long computePlottedStringSize(char *s, double *xlen, double *ylen,
                              double *xcen, double *ycen, unsigned long mode)
/* Computes the size of a string in user coordinates or device coordinates */
{
  int length, justif, xpos, ypos;
  double xsave, ysave;
  double extent[4];
  double xlen0, ylen0, xcen0, ycen0;
  char *s2;
  int i;

  if (!s || !strlen(s))
    return 0;
  translate_hershey_codes(s);
  if (!(length = strlen(s)))
    return 0;

  s2 = malloc(sizeof(char) * (length + 1));
  for (i=0; i<length; i++) {
    if ((s[i] == 'g') || (s[i] == 'j') || (s[i] == 'p') || (s[i] == 'q') || (s[i] == 'y'))  {
      s2[i] = 'x';
    } else {
      s2[i] = s[i];
    }
  }
  s2[length] = 0;

  get_position(&xsave, &ysave);
  xpos = 0;
  ypos = 0;

  justif = RETURN_LENGTH;
  psymbol1(xpos, ypos, s2, (xright - xleft) * character_size,
           character_aspect, device_aspect, 0.0,
           character_tilt, length, justif, extent);
  free(s2);
  xlen0 = fabs(extent[0]);
  ylen0 = fabs(extent[1]);
  xcen0 = fabs(extent[2]);
  ycen0 = fabs(extent[3]);

  if (!(mode & COMPPLOTTEDSIZE_NOROTATE) && character_angle)
    {
      xlen0 *= xscale;
      ylen0 *= yscale;
      rotate(&xlen0, &ylen0, character_angle, 0.0, 0.0);
      xlen0 = fabs(xlen0 / xscale);
      ylen0 = fabs(ylen0 / yscale);
      xcen0 *= xscale;
      ycen0 *= yscale;
      rotate(&xcen0, &ycen0, character_angle, 0.0, 0.0);
      xcen0 = fabs(xcen0 / xscale);
      ycen0 = fabs(ycen0 / yscale);
    }
  if (mode & COMPPLOTTEDSIZE_DEVICEUNITS)
    {
      xlen0 *= xscale;
      ylen0 *= yscale;
      xcen0 *= xscale;
      ycen0 *= yscale;
    }
  if (xlen)
    *xlen = xlen0;
  if (ylen)
    *ylen = ylen0;
  if (xcen)
    *xcen = xcen0;
  if (ycen)
    *ycen = ycen0;

  pmove(xsave, ysave);
  return 1;
}

double computePlottedStringLength(char *label, unsigned long mode)
{
  double dx, dy, dummy;
  if (!computePlottedStringSize(label, &dx, &dy, &dummy, &dummy, mode))
    return 0;
  if (fabs(fabs(sin(character_angle * PI / 180.0)) - 1) < 1e-6)
    dx = dy;
#if DEBUG
  fprintf(stderr, "computePlottedStringLength(%s) : %le\n", label, dx);
#endif
  return dx;
}

void plotTitle(char *label, long lowerTitle, long titleOnTop,
               double scale, double offset, long thickness, long linetype)
{
  struct termentry *t = &term_tbl[term];
  double y, dy, cs = .02;
  long orig_linetype, lockSize = 0;

#if DEBUG
  fprintf(stderr, "plotTitle\n");
#endif
  if (!label || !strlen(label))
    return;
  if (titleOnTop)
    {
      dy = yrange / (qmax - qmin) * (wqmax - qmax) / 2;
      y = ymax + (lowerTitle ? dy * 0.5 : dy * 1.5);
    }
  else
    {
      if (lowerTitle == 0)
        {
          dy = yrange / (qmax - qmin) * (wqmax - qmax);
          y = ymax + dy / 2;
        }
      else
        {
          dy = yrange / (qmax - qmin) * (qmin - wqmin) / 3;
          y = ymin - dy * 2.5;
        }
    }
  y += offset / (qmax - qmin) * (ymax - ymin);
  dy *= scale / 1.45;
  set_linethickness(thickness);
  orig_linetype = get_linetype();
  if (linetype > 0)
    set_linetype(linetype);

  if (!fontsize.autosize)
    {
      cs = character_size;
      if ((!lowerTitle) && (fontsize.topline > 0))
        {
          character_size = fontsize.topline;
          lockSize = 1;
        }
      else if ((lowerTitle) && (fontsize.title > 0))
        {
          character_size = fontsize.title;
          lockSize = 1;
        }
      else if (fontsize.all > 0)
        {
          character_size = fontsize.all;
          lockSize = 1;
        }
    }

  plotStringInBox(label, (xmin + xmax) / 2.0, y, xmax - xmin, dy, 0, lockSize);

  if (!fontsize.autosize)
    {
      character_size = cs;
    }

  if (linetype > 0)
    set_linetype(orig_linetype);
  set_linethickness(1);
  /*This move command is required because postscript files are invalid without a move after a linethinkness change. I don't know why this is.*/
  (*t->move)(0, 0);
}

int translate_hershey_codes(char *s)
{
  char *ptr;
  long has_hershey = 0;
  static char *code = "\003\002\017\005\012\016\006\010\004j\033l\032\001opq\007\011\015\013\014wx\034z0\035\036\037456789";
  /* mode:      normal super  sub  greek roman special end_special backspace  */
  /* character:   n      a     b     g     r     s          e          h      */
  /* code:       \001   \003  \002  \006  \007  \011      \012      \010     */
  /* mode:      bigger smaller    +vertical  -vertical  +aspect  -aspect     */
  /* character:   i     d            u           v         t        f        */
  /* code:      \004  \005         \013        \014       \015     \016      */

  // 15=\017 = c = cryllic
  // 26=\032 = m = mathlow
  // 27=\033 = k = greek (not the original font)
  // 28=\034 = y = symbolic (not the original font)
  // 29=\035 = 1 = rowmans
  // 30=\036 = 2 = rowmand
  // 31=\037 = 3 = rowmant
  ptr = s;
  while (*ptr)
    {
      if (*ptr == '$')
        {
          if (*ptr == *(ptr + 1))
            {
              /* literal dollar sign ($$) */
              strcpy_ss(ptr, ptr + 1);
            }
          else if (ptr != s && *(ptr - 1) == '\\')
            {
              /* escaped dollar sign */
              strcpy_ss(ptr - 1, ptr);
            }
          else if (isalpha(ptr[1]))
            {
              ptr[1] = code[tolower(ptr[1]) - 'a'];
              strcpy_ss(ptr, ptr + 1);
              has_hershey = 1;
            }
          else if (isdigit(ptr[1]))
            {
              ptr[1] = code[tolower(ptr[1]) - 22];
              strcpy_ss(ptr, ptr + 1);
              has_hershey = 1;
            }
        }
      ++ptr;
    }
  return (has_hershey);
}

void get_char_size(
                   double *h,      /* in units of xright-xleft (or user's coords) */
                   /* i.e., h*(xright-left)=width_of_W_in_pixels */
                   double *v,      /* in units of h */
                   int user_coords /* logical: 0 (1) */
                   )
{
  *h = character_size;
  *v = character_size * character_aspect;
  if (user_coords)
    {
      *h *= xrange;
      *v *= (1.0 * xright - xleft) / (1.0 * ytop - ybot) * yrange;
    }
}

void set_default_char_size()
{
  character_size = DEFAULT_CHARACTER_SIZE;
  character_aspect = 1.0;
}

void set_char_size(
                   double h,       /* in units of xright-xleft (or user's coords) */
                   /* i.e., h*(xright-left)=width_of_W_in_pixels */
                   double v,       /* in units of h */
                   int user_coords /* logical: 0 (1) */
                   )
{
  if (user_coords)
    {
      h /= xrange;
      v /= (1.0 * xright - xleft) / (1.0 * ytop - ybot) * yrange;
    }

  character_size = h;
  if (h)
    character_aspect = v / h;
  else
    character_aspect = 1;
}

void fix_char_size(
                   double h,        /* in units of xright-xleft (or user's coords) */
                   /* i.e., h*(xright-left)=width_of_W_in_pixels */
                   double v,        /* in units of h */
                   int user_coords, /* logical: 0 (1) */
                   short mode)
{
  if (user_coords)
    {
      h /= xrange;
      v /= (1.0 * xright - xleft) / (1.0 * ytop - ybot) * yrange;
    }

  if (fontsize.autosize)
    {
      character_size = h;
    }
  else
    {
      if (fontsize.all > 0)
        {
          character_size = fontsize.all;
        }
      else
        {
          character_size = h;
        }
      if ((mode == 1) && (fontsize.ylabel > 0))
        {
          character_size = fontsize.ylabel;
        }
      else if ((mode == 2) && (fontsize.xlabel > 0))
        {
          character_size = fontsize.xlabel;
        }
      else if ((mode == 3) && (fontsize.xticks > 0))
        {
          character_size = fontsize.xticks;
        }
      else if ((mode == 4) && (fontsize.yticks > 0))
        {
          character_size = fontsize.yticks;
        }
      else if ((mode == 5) && (fontsize.legend > 0))
        {
          character_size = fontsize.legend;
        }
    }

  if (character_size)
    character_aspect = v / h;
  else
    character_aspect = 1;
}

void label_character_size(int turn_on)
{
  return;
}

void char_angle(double rigid, double deform)
{
  character_angle = rigid;
  character_tilt = deform;
}

void char_scale(double xfact, double yfact, short mode)
{
  if (fontsize.autosize)
    {
      character_size *= xfact;
    }
  else
    {
      if ((fontsize.all > 0) && (mode != 5))
        {
          character_size = fontsize.all;
        }
      else
        {
          character_size *= xfact;
        }
      if ((mode == 1) && (fontsize.ylabel > 0))
        {
          character_size = fontsize.ylabel;
        }
      else if ((mode == 2) && (fontsize.xlabel > 0))
        {
          character_size = fontsize.xlabel;
        }
      else if ((mode == 3) && (fontsize.xticks > 0))
        {
          character_size = fontsize.xticks;
        }
      else if ((mode == 4) && (fontsize.yticks > 0))
        {
          character_size = fontsize.yticks;
        }
    }
  character_aspect *= yfact / xfact;
}

static int lineThickness;
int set_linethickness(int lthickness)
{
  struct termentry *t = &term_tbl[term];
  int oldThickness = lineThickness;
  if ((lthickness < 1) || (lthickness > 9))
    return (oldThickness);
  lineThickness = lthickness;
  (*t->line_thickness)(lthickness);
  mpl_force_linetype = 1;
  return (oldThickness);
}

int set_linetype(int ltype)
{
  int last_linetype;
  struct termentry *t = &term_tbl[term];
  last_linetype = mpl_linetype;
  if (ltype == PRESET_LINETYPE)
    return PRESET_LINETYPE;
  /*
    if ((ltype==PRESET_LINETYPE) && (!mpl_force_linetype))
    return PRESET_LINETYPE;
  */
  if (ltype < 0)
    ltype = 0;
  if ((mpl_force_linetype) || (mpl_linetype != ltype))
    {
      (*t->linetype)(mpl_linetype = ltype);
      mpl_force_linetype = 0;
    }
  return last_linetype;
}

int get_linetype(void)
{
  return (mpl_linetype);
}

void draw_box(double xl, double xh, double yl, double yh)
{
  struct termentry *t = &term_tbl[term];
  int ixl, ixh, iyl, iyh;

  check_scales("draw_box");
  (*t->move)(ixl = map_x(xl), iyl = map_y(yl));
  (*t->vector)(ixl, iyh = map_y(yh));
  (*t->vector)(ixh = map_x(xh), iyh);
  (*t->vector)(ixh, iyl);
  (*t->vector)(ixl, iyl);
}

void plot_error_bars(double *x, double *y, double *sx, double *sy, long n, int mode, int line_thickness)
{
  int i, line_type;

  check_scales("plot_error_bars");

  line_type = ((mode & (0xf - 3)) >> 1) + mpl_linetype;
  for (i = 0; i < n; i++)
    plot_eb(x[i], y[i], (sx ? sx[i] : 0.0), (sy ? sy[i] : 0.0), mode & 1, line_type, line_thickness);
  if (mode == 2)
    plot_points(x, y, n, 0, mpl_linetype, 1.0, line_thickness);
}

void plot_eb(double x, double y, double sx, double sy, int mode, int line_type, int line_thickness)
/* plot point with error bars (mode==0) or error boxes (mode!=0) */
{
  static double xf[5], yf[5];
  static double xbar_size, ybar_size;

  xbar_size = 0.005 * yrange;
  ybar_size = 0.005 * xrange;

  if (!mode)
    {
      xf[0] = x - sx;
      xf[1] = x + sx;
      yf[0] = yf[1] = y;
      plot_lines(xf, yf, 2, line_type, line_thickness);

      xf[0] = xf[1] = x;
      yf[0] = y + sy;
      yf[1] = y - sy;
      plot_lines(xf, yf, 2, line_type, line_thickness);
      plot_lines(xf, yf, 2, line_type, line_thickness);

      if (xbar_size != 0)
        {
          if (sy && xbar_size > sy)
            xbar_size = sy;
          xf[0] = xf[1] = x + sx;
          yf[0] = y - xbar_size;
          yf[1] = y + xbar_size;
          plot_lines(xf, yf, 2, line_type, line_thickness);
          xf[0] = xf[1] = x - sx;
          plot_lines(xf, yf, 2, line_type, line_thickness);
        }

      if (ybar_size != 0)
        {
          if (sx && ybar_size > sx)
            ybar_size = sx;
          yf[0] = yf[1] = y + sy;
          xf[0] = x - ybar_size;
          xf[1] = x + ybar_size;
          plot_lines(xf, yf, 2, line_type, line_thickness);
          yf[0] = yf[1] = y - sy;
          plot_lines(xf, yf, 2, line_type, line_thickness);
        }
    }
  else
    {
      xf[0] = x + sx;
      yf[0] = y + sy;
      xf[1] = xf[0];
      yf[1] = y - sy;
      xf[2] = x - sx;
      yf[2] = yf[1];
      xf[3] = xf[2];
      yf[3] = yf[0];
      xf[4] = xf[0];
      yf[4] = yf[0];
      plot_lines(xf, yf, 5, line_type, line_thickness);
    }
}

void widen_window(int make_wide)
{
  static int saved = 0;
  static int clipping[3];

  if (make_wide)
    {
      /*
        xwl_save = xw_left;
        xwr_save = xw_right;
        ywb_save = yw_bot;
        ywt_save = yw_top;
        xw_mins = xw_min;
        xw_maxs = xw_max;
        yw_mins = yw_min;
        yw_maxs = yw_max;
        xw_left  = yw_bot = 0;
        xw_right = t->xmax-1;
        yw_top   = t->ymax-1;
      */
      clipping[0] = clip_points;
      clipping[1] = clip_lines1;
      clipping[2] = clip_lines2;
      clip_points = clip_lines1 = clip_lines2 = 0;
      saved = 1;
    }
  else
    {
      if (!saved)
        bomb("widen_window(0) called without previous widen_window(1) call", NULL);
      /*
        xw_left  = xwl_save;
        xw_right = xwr_save;
        yw_bot   = ywb_save;
        yw_top   = ywt_save;
        xw_min = xw_mins ;
        xw_max = xw_maxs ;
        yw_min = yw_mins ;
        yw_max = yw_maxs ;
      */
      clip_points = clipping[0];
      clip_lines1 = clipping[1];
      clip_lines2 = clipping[2];
      saved = 0;
    }
}

void check_scales(char *caller)
{
  static char last_caller[100] = "<none>";

  if (!users_coords_set || xscale <= 0 || yscale <= 0)
    {
      fprintf(stderr, "error: coordinate conversion error\n");
      fprintf(stderr, "user's coordinates of plot region: [%e, %e] x [%e, %e]\n",
              xmin, ymax, ymin, ymax);
      fprintf(stderr, "plot region in unit coordinates: [%e, %e] x [%e, %e]\n",
              pmin, pmax, qmin, qmax);
      fprintf(stderr, "physical coordinate limits: [%d, %d] x [%d, %d]\n",
              xw_left, xw_right, yw_bot, yw_top);
      fprintf(stderr, "error occured in routine %s\n", caller);
      fprintf(stderr, "previous caller was %s\n", last_caller);
      fprintf(stderr, "\7\7please record this printout and email to soliday@anl.gov\n");
      exit(1);
    }
  strcpy_ss(last_caller, caller);
}

void shade_box(long shade, double xl, double xh, double yl, double yh)
{
  struct termentry *t = &term_tbl[term];
  if (!(t->flags & TERM_POLYFILL))
    bomb("can't do shading for selected device", NULL);
  if (!(t->fillbox))
    bomb("can't do shading for selected device--routine missing", NULL);
  (*t->fillbox)(shade, map_x(xl), map_x(xh), map_y(yl), map_y(yh));
  (*t->linetype)(mpl_linetype);
#if defined(_X86_) && defined(SOLARIS)
  sleep(.0001);
#endif
}

char *gs_device_arguments(char *devarg, long get)
{
  static char *device_args = NULL;
  char *copy;
  if (!get)
    {
      cp_str(&device_args, devarg);
      return (devarg);
    }
  else
    {
      if (device_args)
        {
          cp_str(&copy, device_args);
          return (copy);
        }
      return (NULL);
    }
}

static char **deviceArgv;
static long deviceArgc;
void setDeviceArgv(char **argv, long argc)
{
  long i;
  if (argc <= 0)
    {
      deviceArgc = 0;
      deviceArgv = NULL;
      return;
    }
  if (!(deviceArgv = (char **)malloc(sizeof(*deviceArgv) * argc)))
    bomb("error setting device arguments", NULL);
  for (i = 0; i < argc; i++)
    cp_str(deviceArgv + i, argv[i]);
  deviceArgc = argc;
}

char **getDeviceArgv(long *argc)
{
  long i;

  char **argv;
  if (!deviceArgc)
    return NULL;
  *argc = deviceArgc;
  if (!(argv = (char **)malloc(sizeof(*argv) * deviceArgc)))
    bomb("error setting device arguments", NULL);
  for (i = 0; i < deviceArgc; i++)
    cp_str(argv + i, deviceArgv[i]);
  return argv;
}

long SDDS_ReadLineColorTable(LINE_COLOR_TABLE *LCT, char *filename)
{
  SDDS_DATASET SDDSin;

  if (!LCT)
    {
      SDDS_SetError("SDDS_ReadLineColorTable: Null LINE_COLOR_TABLE pointer");
      return 0;
    }
  if (!filename || !strlen(filename))
    {
      SDDS_SetError("SDDS_ReadLineColorTable: no filename given");
      return 0;
    }
  if (!SDDS_InitializeInput(&SDDSin, filename) ||
      SDDS_ReadPage(&SDDSin) != 1)
    return 0;
  if ((LCT->nEntries = SDDS_RowCount(&SDDSin)) <= 1)
    {
      SDDS_SetError("SDDS_ReadLineColorTable: too few entries (need at least 1)");
      return 0;
    }
  if (!(LCT->red = SDDS_Realloc(NULL, sizeof(*(LCT->red)) * LCT->nEntries)) ||
      !(LCT->green = SDDS_Realloc(NULL, sizeof(*(LCT->green)) * LCT->nEntries)) ||
      !(LCT->blue = SDDS_Realloc(NULL, sizeof(*(LCT->blue)) * LCT->nEntries)))
    {
      SDDS_SetError("SDDS_ReadLineColorTable: memory allocation failure");
      return 0;
    }

  if (!(LCT->red = SDDS_GetColumnInLong(&SDDSin, "red")) &&
      !(LCT->red = SDDS_GetColumnInLong(&SDDSin, "Red")))
    {
      SDDS_SetError("SDDS_ReadLineColorTable: no red or Red column found");
      return 0;
    }

  if (!(LCT->green = SDDS_GetColumnInLong(&SDDSin, "green")) &&
      !(LCT->green = SDDS_GetColumnInLong(&SDDSin, "Green")))
    {
      SDDS_SetError("SDDS_ReadLineColorTable: no green or Green column found");
      return 0;
    }

  if (!(LCT->blue = SDDS_GetColumnInLong(&SDDSin, "blue")) &&
      !(LCT->blue = SDDS_GetColumnInLong(&SDDSin, "Blue")))
    {
      SDDS_SetError("SDDS_ReadLineColorTable: no blue or Blue column found");
      return 0;
    }

  SDDS_Terminate(&SDDSin);

  return 1;
}

long SDDS_ReadLineTypeTable(LINE_TYPE_TABLE *LTT, char *filename)
{
  SDDS_DATASET SDDSin;
  int i, j, index, token;
  char **dashString, *str1;

  if (!LTT)
    {
      SDDS_SetError("SDDS_ReadLineTypeTable: Null LINE_COLOR_TABLE pointer");
      return 0;
    }
  LTT->typeFlag = 0x0000;

  if (!filename || !strlen(filename))
    {
      SDDS_SetError("SDDS_ReadLineTypeTable: no filename given");
      return 0;
    }
  if (!SDDS_InitializeInput(&SDDSin, filename) ||
      SDDS_ReadPage(&SDDSin) != 1)
    return 0;
  if ((LTT->nEntries = SDDS_RowCount(&SDDSin)) <= 1)
    {
      SDDS_SetError("SDDS_ReadLineTypeTable: too few entries (need at least 1)");
      return 0;
    }

  if ((SDDS_GetColumnIndex(&SDDSin, "thickness") != -1) ||
      (SDDS_GetColumnIndex(&SDDSin, "Thickness") != -1))
    {
      if (!(LTT->thickness = SDDS_Realloc(NULL, sizeof(*(LTT->thickness)) * LTT->nEntries)))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: memory allocation failure");
          return 0;
        }
      else
        {
          LTT->thickness = (SDDS_GetColumnIndex(&SDDSin, "thickness") != -1) ? SDDS_GetColumnInLong(&SDDSin, "thickness") : SDDS_GetColumnInLong(&SDDSin, "Thickness");
          LTT->typeFlag |= LINE_TABLE_DEFINE_THICKNESS;
        }
    }

  index = ((i = SDDS_GetColumnIndex(&SDDSin, "dash")) >= 0) ? i : SDDS_GetColumnIndex(&SDDSin, "Dash");
  if (index >= 0)
    {
      if (SDDS_GetColumnType(&SDDSin, index) != SDDS_STRING)
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: dash cloumn must be SDDS STRING type");
          return 0;
        }
      if (!(dashString = SDDS_Realloc(NULL, SDDS_type_size[SDDS_STRING - 1] * LTT->nEntries)))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: memory allocation failure");
          return 0;
        }
      else
        {
          dashString = (i >= 0) ? SDDS_GetColumn(&SDDSin, "dash") : SDDS_GetColumn(&SDDSin, "Dash");
          if (!(LTT->dash = SDDS_Realloc(NULL, sizeof(*(LTT->dash)) * LTT->nEntries)))
            {
              SDDS_SetError("SDDS_ReadLineTypeTable: memory allocation failure");
              return 0;
            }

          for (i = 0; i < LTT->nEntries; i++)
            {

              for (j = 0; j < 5; j++)
                LTT->dash[i].dashArray[j] = 0;
              j = 0;
              while ((j < 5) && (str1 = get_token_t(dashString[i], " ,:;")))
                {
                  if (!get_int(&token, str1))
                    {
                      SDDS_SetError("SDDS_ReadLineTypeTable: wrong dash definition string");
                      return 0;
                    }
                  LTT->dash[i].dashArray[j] = (char)token;
                  j++;
                }
              LTT->typeFlag |= LINE_TABLE_DEFINE_DASH;
            }
        }
    }

  if ((SDDS_GetColumnIndex(&SDDSin, "red") != -1) || (SDDS_GetColumnIndex(&SDDSin, "Red") != -1) ||
      (SDDS_GetColumnIndex(&SDDSin, "green") != -1) || (SDDS_GetColumnIndex(&SDDSin, "Green") != -1) ||
      (SDDS_GetColumnIndex(&SDDSin, "blue") != -1) || (SDDS_GetColumnIndex(&SDDSin, "Blue") != -1))
    {
      if (!(LTT->red = SDDS_Realloc(NULL, sizeof(*(LTT->red)) * LTT->nEntries)) ||
          !(LTT->green = SDDS_Realloc(NULL, sizeof(*(LTT->green)) * LTT->nEntries)) ||
          !(LTT->blue = SDDS_Realloc(NULL, sizeof(*(LTT->blue)) * LTT->nEntries)))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: memory allocation failure");
          return 0;
        }

      if (!(LTT->red = SDDS_GetColumnInLong(&SDDSin, "red")) &&
          !(LTT->red = SDDS_GetColumnInLong(&SDDSin, "Red")))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: no red or Red column found");
          return 0;
        }

      if (!(LTT->green = SDDS_GetColumnInLong(&SDDSin, "green")) &&
          !(LTT->green = SDDS_GetColumnInLong(&SDDSin, "Green")))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: no green or Green column found");
          return 0;
        }

      if (!(LTT->blue = SDDS_GetColumnInLong(&SDDSin, "blue")) &&
          !(LTT->blue = SDDS_GetColumnInLong(&SDDSin, "Blue")))
        {
          SDDS_SetError("SDDS_ReadLineTypeTable: no blue or Blue column found");
          return 0;
        }
      LTT->typeFlag |= LINE_TABLE_DEFINE_COLOR;
    }

  if (!LTT->typeFlag)
    {
      SDDS_SetError("SDDS_ReadLineTypeTable: no linetype definition (thickness or dash or color) found");
      return 0;
    }
  SDDS_Terminate(&SDDSin);

  return 1;
}

void make_intensity_bar(long n_shades, long shadeOffset, long reverse,
                        double min_value, double max_value,
                        double hue0, double hue1, char *colorSymbol,
                        char *colorUnits, long tickLabelThickness,
                        double labelsize, double unitsize, double xadjust)
{
  long i, shade, pen, locksize=0;
  double xmin, xmax, ymin, ymax, allowedSpace;
  double yrange, xrange, dy, y, yave;
  double xl, yl, xh, yh;
  double xb[5], yb[5];

  get_mapping(&xmin, &xmax, &ymin, &ymax);
  if (!(yrange = ymax - ymin))
    bomb("y range is zero (make_intensity_bar)", NULL);
  yrange *= .8;
  yave = (ymin + ymax) / 2;
  ymin = yave - yrange / 2;
  ymax = yave + yrange / 2;
  if (!(xrange = xmax - xmin))
    bomb("x range is zero (make_intensity_bar)", NULL);
  xl = xmin + xrange * (1.055 + xadjust / 1000);
  xh = xmin + xrange * (1.095 + xadjust / 1000);
  allowedSpace = 2 * (xh - xl);
  yl = ymin;
  yh = ymax;
  dy = (yh - yl) / (n_shades + 1);

  set_clipping(0, 0, 0);
  pen = set_linetype(0);
  set_linetype(pen);
  if (reverse)
    {
      SWAP_DOUBLE(hue0, hue1);
    }
  if (colorSymbol)
    {
      set_linethickness(tickLabelThickness);
      if ((fontsize.all > 0) || (fontsize.legend > 0)) {
        locksize = 1;
      }
      if (colorUnits)
        {
          char units[256];
          double hsize, vsize;
          get_char_size(&hsize, &vsize, 1);
          fix_char_size(hsize, vsize, 1, 5);
          sprintf(units, "(%s)", colorUnits);
          plotStringInBox(units, (xh + xl) / 2.0, yh + (yh - yl) * 0.05 + ((yh - yl) * 0.03 * (unitsize - 1)), allowedSpace * unitsize * 0.5, (yh - yl) * 0.03 * unitsize, 0, locksize);
          plotStringInBox(colorSymbol, (xh + xl) / 2.0, yh + (yh - yl) * 0.05 + ((yh - yl) * 0.03 * (unitsize - 1)) + vsize * 1.5, allowedSpace * unitsize * 0.9, (yh - yl) * 0.03 * unitsize, 0, locksize);
        }
      else
        {
          double hsize, vsize;
          get_char_size(&hsize, &vsize, 1);
          fix_char_size(hsize, vsize, 1, 5);
          plotStringInBox(colorSymbol, (xh + xl) / 2.0, yh + (yh - yl) * 0.05 + ((yh - yl) * 0.03 * (unitsize - 1)) + vsize * 1.5, allowedSpace * unitsize * 0.9, (yh - yl) * 0.03 * unitsize, 0, locksize);
        }
      set_linethickness(0);
    }
  for (i = 0; i <= n_shades; i++)
    {
      y = yl + dy * i;
      /* shade = (hue1-hue0)*((100.0*i)/n_shades)+100*hue0;*/
      if (n_shades <= 100)
        {
          shade = (hue1 - hue0) * (i) + (n_shades * hue0) + shadeOffset;
        }
      else
        {
          shade = (hue1 - hue0) * ((100.0 * i) / n_shades) + ((100 * hue0) / n_shades) + shadeOffset;
        }
      shade_box(shade, xl, xh, y, y + dy);
    }

  set_linetype(pen);
  xb[0] = xb[1] = xb[4] = xl;
  yb[0] = yb[3] = yb[4] = yl;
  xb[2] = xb[3] = xh;
  yb[1] = yb[2] = yh;
  plot_lines(xb, yb, 5, PRESET_LINETYPE, 0);

  set_clipping(1, 1, 1);

  make_scale(1,                                                    /* plane */
             0,                                                    /* log? */
             1,                                                    /* labels? */
             0.003,                                                /* tick fraction */
             0.0,                                                  /* spacing (autospacing) */
             0,                                                    /* linetype */
             0,                                                    /* thickness */
             tickLabelThickness,                                   /* thickness for numeric ticks */
             (max_value - min_value) / (yh - yl),                  /* factor */
             1,                                                    /* hide factor */
             0,                                                    /* don't invert scale */
             max_value - (max_value - min_value) / (yh - yl) * yh, /* offset */
             0,                                                    /* modulus */
             1.0 * labelsize,                                      /* label frac */
             0,                                                    /* subticks */
             0.0,
             0,                        /* subtick linetype */
             0,                        /* subthickness */
             0,                        /* side */
             yl,                       /* range min */
             yh,                       /* range max */
             xl,                       /* position */
             allowedSpace * labelsize, /* space for labels */
             0,                        /* adjust for ticks */
             NULL, 1, 0, 0, 0 /* label */,
             1 /* no subtick label for log scale*/
             );
  make_scale(1,     /* plane */
             0,     /* log? */
             0,     /* labels? */
             0.003, /* tick fraction */
             0.0,   /* spacing (autospacing) */
             0,     /* linetype */
             0,     /* thickness */
             tickLabelThickness,
             (max_value - min_value) / (yh - yl),                  /* factor */
             1,                                                    /* hide factor */
             0,                                                    /* don't invert scale */
             max_value - (max_value - min_value) / (yh - yl) * yh, /* offset */
             0,                                                    /* modulus */
             0.0,                                                  /* label frac */
             0,                                                    /* subticks */
             0.0,
             0,                        /* subtick linetype */
             0,                        /* subthickness */
             1,                        /* side */
             yl,                       /* range min */
             yh,                       /* range max */
             xl,                       /* position */
             allowedSpace * labelsize, /* space for labels */
             0,                        /* adjust for ticks */
             NULL, 1, 0, 0, 0 /* label */,
             1 /* no subtick label for log scale */
             );
}

void SetupFontSize(FONT_SIZE *fs)
{
  fontsize.autosize = fs->autosize;
  fontsize.all = fs->all;
  fontsize.legend = fs->legend;
  fontsize.xlabel = fs->xlabel;
  fontsize.ylabel = fs->ylabel;
  fontsize.xticks = fs->xticks;
  fontsize.yticks = fs->yticks;
  fontsize.title = fs->title;
  fontsize.topline = fs->topline;
}
