/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* prototypes for this file are in plot_contours4.prot */
/* file: plot_contours
 * contents: go_plot_contours(), label_contours(), label_contour()
 *
 * Michael Borland, 1989, 1991
 $Log: not supported by cvs2svn $
 Revision 1.15  2005/08/30 21:32:05  soliday
 Updated to compile with recent changes to sddsplot functions.

 Revision 1.14  2005/04/26 22:25:33  shang
 added xlabelScale and ylabelScale arguments

 Revision 1.13  2004/09/14 19:03:14  soliday
 Updated to work with the -ticksettings=xtime option.

 Revision 1.12  2003/09/02 19:32:36  soliday
 Cleaned up code for Linux.

 Revision 1.11  2003/08/16 19:38:55  borland
 More fixes for the frame_end() business. Frame ending is only done in the
 main routine now.  Removed special frame_end code for the columnMatch
 mode, since it shouldn't be needed now.

 Revision 1.10  2002/12/04 17:55:20  soliday
 Fixed the shape option when used with the contour option.

 Revision 1.9  2002/08/14 17:35:28  soliday
 Added Open License

 Revision 1.8  2001/08/13 20:12:39  soliday
 Updated function calls due to changes in sddsplot.

 Revision 1.7  2001/06/04 20:21:14  soliday
 Added the -layout option.

 Revision 1.6  2000/06/02 21:14:35  soliday
 I had to adjust some function calls so that they work with the new thickness
 argument.

 Revision 1.5  2000/04/10 19:01:06  borland
 -shapes option now works again, and uses SDDS files.

 Revision 1.4  1999/07/23 15:53:19  soliday
 Added WIN32 support

 Revision 1.3  1999/06/03 20:42:39  soliday
 Removed compiler warnings under Linux.

 Revision 1.2  1995/09/05 21:08:50  saunders
 First test release of the SDDS1.5 package.

 */
#include "mdb.h"
#include "graph.h"
#include "contour.h"
#if defined(_WIN32)
#include <windows.h>
#define sleep(sec) Sleep(sec * 1000)
#else
#include <unistd.h>
#endif

void PlotShapesData(SHAPE_DATA *shape, long shapes, double xmin, double xmax, double ymin, double ymax);

void go_plot_contours(
    char *device, char *title, char *xvar, char *yvar, char *topline,
    double **data, double xmin, double xmax, double ymin, double ymax,
    double dx, double dy, long nx, long ny,
    double *contour_level, long n_contours, long contour_label_interval,
    long contour_label_offset, long layout[2], long ix, long iy,
    char *shapes, int *pen, long flags, long pause_interval,
    SHAPE_DATA *shape, long nshapes, unsigned long long tsetFlags, double xlabelScale, double ylabelScale,
    short noSetup, long thickness, long fill_screen)
{
    double map[4], average, spread;
    double pmin, pmax, qmin, qmax;
    double wpmin, wpmax=0, wqmin, wqmax=0;
    long title_at_top=0, pen0=0;
    char newLabel[50];
    time_t timeValue;

    if (!noSetup) {
      if (!(flags&DEVICE_DEFINED)) 
        change_term(device, strlen(device));
      if ((ix == 0) && (iy == 0))
        graphics_on();
      if (layout[0] && layout[1]) {
        wpmin = (1.0*ix+0)/layout[0];
        wpmax = (1.0*ix+1)/layout[0];
        wqmin = (layout[1]*1.0 - 1 - iy)/layout[1];
        wqmax = (layout[1]*1.0 - 0 - iy)/layout[1];
        set_wspace(wpmin, wpmax, wqmin, wqmax);
        pmin = (wpmax - wpmin) * .15 + wpmin;
        pmax = wpmax - (wpmax - wpmin) * .1;
        qmin = (wqmax - wqmin) * .17 + wqmin;
        qmax = wqmax - (wqmax - wqmin) * .08;
        set_pspace(pmin, pmax, qmin, qmax);
      }
      if (contour_label_interval<0) {
        get_pspace(&pmin, &pmax, &qmin, &qmax);
        set_pspace(pmin, 0.9 * wpmax, qmin, 0.85 * wqmax);
      }
      if (flags&TITLE_AT_TOP) {
        /* adjust pspace to make room at top for the title */
        get_pspace(&pmin, &pmax, &qmin, &qmax);
        spread = qmax-qmin;
        qmax -= 0.04*spread;
        qmin -= 0.04*spread;
        /*    qmax -= 0.04;
              qmin -= 0.04;*/
        set_pspace(pmin, pmax, qmin, qmax);
        title_at_top = 1;
      }
      if (fill_screen)
        set_pspace(0, 1, 0, 1);
      set_clipping(1, 1, 1);

      pen0 = set_linetype(0);
      set_linetype(pen0);
      set_linethickness(thickness);
      set_linetype(pen[0]);

      if (flags&EQUAL_ASPECT1) 
        set_aspect(1.0L);
      else if (flags&EQUAL_ASPECT_1)
        set_aspect(-1.0L);

      get_mapping(map, map+1, map+2, map+3);
      if (map[0]==map[1]) {
        average  = (xmax+xmin)/2;
        spread   = (xmax-xmin)/2;
        if (fill_screen) {
          map[0]   = average - spread;
          map[1]   = average + spread;
        } else {
          map[0]   = average - spread*1.05;
          map[1]   = average + spread*1.05;
        }
      }
      if (map[2]==map[3]) {
        average  = (ymax+ymin)/2;
        spread   = (ymax-ymin)/2;
        if (fill_screen) {
          map[2]   = average - spread;
          map[3]   = average + spread;
        } else {
          map[2]   = average - spread*1.05;
          map[3]   = average + spread*1.05;
        }
      }
      set_mapping(map[0], map[1], map[2], map[3]);
      get_pspace(&pmin, &pmax, &qmin, &qmax);
      get_wspace(&wpmin, &wpmax, &wqmin, &wqmax);
      

      if (!(flags&NO_BORDER)) {
        border();
        if (!(flags&NO_SCALES)) {
          if (tsetFlags&TICKSET_XTIME) {
            timeValue = map[0];
            if (timeValue!=DBL_MAX)
              sprintf(newLabel, "Time starting %s", ctime(&timeValue));
            else
              sprintf(newLabel, "Undefined time values!");
            delete_chars(newLabel, "\n");
            makeTimeScales(0, 0.02, 0, map[2], map[2], newLabel, 
                           0, 0.67*(qmin-wqmin)*(map[3]-map[2])/(qmax-qmin), 
                           0, 1, 1, thickness, thickness, thickness);
          } else {
            make_scales_with_label(0, 0, 1, 0.0, 0.02, 0.0, 
                                   0.0, 1.0, 0, 0, 
                                   thickness, thickness, 0, 
                                   thickness, xvar, 0, thickness, xlabelScale);
          }
          if (!(flags&NO_YSCALES)) {
            if (tsetFlags&TICKSET_YTIME) {
              timeValue = map[2];
              if (timeValue!=DBL_MAX)
                sprintf(newLabel, "Time starting %s", ctime(&timeValue));
              else
                sprintf(newLabel, "Undefined time values!");
              delete_chars(newLabel, "\n");
              makeTimeScales(1, 0.0125, 0, map[0], map[0], newLabel, 
                             0, (pmin-wpmin)*(map[1]-map[0])/(pmax-pmin), 
                             0, 1, 1, thickness, thickness, thickness);
            } else {
              make_scales_with_label(1, 0, 1, 0.0, 0.0125, 0.0, 
                                     0.0, 1.0, 0, 0, 
                                     thickness, thickness, 0, 
                                     thickness, yvar, 0 ,thickness, ylabelScale);
            }
          }
        }
      }
      if (!(flags&NO_LABELS)) {
        plotTitle(title, 1, title_at_top, 1.0, 0.0, thickness, 0);
        if (contour_label_interval>=0)
          plotTitle(topline, 0, 0, 1.0, 0.0, thickness, 0);
      }
    }

    draw_contours(data, xmin, xmax, ymin, ymax, nx, ny, contour_level, n_contours, thickness, flags);
    if (contour_label_interval!=0) {
        set_linetype(pen[2]);
        set_linethickness(thickness);
        label_contours(data, nx, ny, contour_level, n_contours,
            contour_label_interval, contour_label_offset,
            xmin, (double)dx, ymin, (double)dy);
        }

    if (nshapes)
      PlotShapesData(shape, nshapes, xmin, xmax, ymin, ymax);

    set_linetype(pen0);
    set_linethickness(1);

    if (flags&DATE_STAMP)
        time_date_stamp();

    }
    
void label_contours(
    double **data,
    long nx,
    long ny,
    double *contour_level,
    long n_contours,
    long label_interval,
    long label_offset,
    double xmin,
    double dx,
    double ymin,
    double dy
    )
{
    register long i;
    char s[100], format[100];
    double max_contour, min_contour, value;
    long label_mode = 0;

    if (label_interval==0)
        return;

    widen_window(1);

    if (label_interval<0) {
        label_mode = 1;
        label_interval = -label_interval;
        }

    max_contour = -(min_contour = FLT_MAX);
    for (i=0; i<n_contours; i++) {
        if ((value=contour_level[i])>max_contour)
            max_contour = value;
        if (value<min_contour)
            min_contour = value;
        }

    value = fabs((max_contour-min_contour)/n_contours);
    if (value>1e3 || value<1e-2) 
        strcpy(format, "$d%.2le$i");
    else if (value>1e2)
        strcpy(format, "%.0lf");
    else if (value>1e1)
        strcpy(format, "%.1lf");
    else
        strcpy(format, "%.2lf");

    for (i=label_offset; i<n_contours; i+=label_interval) {
        sprintf(s, format, value=contour_level[i]);
        label_contour(data, nx, ny, (double)value, s, xmin, dx, ymin, dy,
                2*(double)((max_contour-min_contour)/n_contours), label_mode);
        }

    vertical_print(0);
    widen_window(0);
    }
    
void label_contour(
    double **data,
    long nx,
    long ny,
    double value,
    char *string,
    double xmin,
    double dx,
    double ymin,
    double dy,
    double error_limit,
    long label_mode  
    )
{
/* long label_mode    0 -> on contours,  1 -> around border */
    register double distance, best_distance;
    register long iy;
    long ix, ix_best, iy_best, ix_nbest=0, iy_nbest=0;
    double x, y, xmax, ymax, i_int;
    double xd, yd;
    double hoff, voff;

    xmax = xmin + dx*(nx-1);
    ymax = ymin + dy*(ny-1);

    if (label_mode==0) {
        distance = best_distance = 1.0E37;
        ix_best = iy_best = -1;
    
        for (ix=1; ix<nx-2; ix++) {
            for (iy=1; iy<ny-2; iy++) {
                distance = fabs(value - data[ix][iy])/error_limit;
                if (distance<1) {
                    distance += sqr((ix-nx/2.)/nx)+sqr((iy-ny/2.)/ny);
                    if (distance<best_distance) {
                        ix_nbest = ix_best; 
                        iy_nbest = iy_best;
                        ix_best = ix;
                        iy_best = iy;
                        best_distance = distance;
                        }
                    }
                }
            }
        ix = ix_best;
        iy = iy_best;
    
        if (ix==-1) {
            ix = 0;
            for (iy=1; iy<ny-2; iy++) {
                distance = fabs(value - data[ix][iy])/error_limit;
                if (distance<1) {
                    distance += sqr((ix-nx/2.)/nx)+sqr((iy-ny/2.)/ny);
                    if (distance<best_distance) {
                        ix_nbest = ix_best; 
                        iy_nbest = iy_best;
                        ix_best = ix;
                        iy_best = iy;
                        best_distance = distance;
                        }
                    }
                }
            if (ix_best==-1) {
                printf("note: failed to find acceptable contour to label for value %e\n", value);
                return;
                }
            ix = ix_best;
            iy = iy_best;
            }

        if (fabs(value-data[ix][iy])>error_limit)
            bomb("algorithmic error (1) in label_contour()", NULL);
    
        x = dx*ix + xmin;
        y = dy*iy + ymin;
        if (ix<nx-1 && data[ix][iy]!=data[ix+1][iy])
            x += dx*(value-data[ix][iy])/(data[ix+1][iy]-data[ix][iy]);
        else if (iy<ny-1 && data[ix][iy]!=data[ix][iy+1])
            y += dy*(value-data[ix][iy])/(data[ix][iy+1]-data[ix][iy]);
    
        if (x>=xmax-dx/2 || y>=ymax-dy/2 || x<xmin+dx/4 || y<ymin+dy/4) {
            ix = ix_nbest;
            iy = iy_nbest;
            if (ix!=-1) {
                x = dx*ix + xmin;
                y = dy*iy + ymin;
                if (ix<nx-1 && data[ix][iy]!=data[ix+1][iy])
                    x += dx*(value-data[ix][iy])/(data[ix+1][iy]-data[ix][iy]);
                else if (iy<ny-1 && data[ix][iy]!=data[ix][iy+1])
                    y += dy*(value-data[ix][iy])/(data[ix][iy+1]-data[ix][iy]);
                if (x>=xmax-dx/2 || y>=ymax-dy/2 || x<xmin+dx/4 || y<ymin+dy/4) {
                    printf("note: label algorithm went outside plot region for value %e\n", value);
                    return;
                    }
                }
            else {
                printf("note: label algorithm went outside plot region for value %e\n", value);
                return;
                }
            }

        xd = x;
        yd = y;
        plot_string(&xd, &yd, string);
        }
    else {
        distance = best_distance = 1.0E37;
        vertical_print(1);

        /* search for end of contour along top edge */
        iy = ny-1;
        get_char_size(&hoff, &voff, 1);
        for (ix=nx-1; ix>0; ix--) {
            if ((data[ix][iy]<value && data[(ix-1)][iy]>=value) ||
                (data[ix][iy]>value && data[(ix-1)][iy]<=value) ) {
                xd = xmin + hoff/3 + dx*(i_int=INTERPOLATE(ix-1, ix, 
                                    data[ix-1][iy], data[ix][iy], value));
                if (i_int>=ix-1 && i_int<=ix) {
                    yd = ymin + (ny-1)*dy + (value>0?voff:voff/2);
                    plot_string(&xd, &yd, string);
                    }
                }
            }
    
        /* search for end of contour along right edge */
        ix = nx-1;
        vertical_print(0);
        get_char_size(&hoff, &voff, 1);
        for (iy=1; iy<ny; iy++) {
            if ((data[ix][iy]<value && value<=data[ix][iy-1]) ||
                (data[ix][iy]>value && value>=data[ix][iy-1]) ) {
                yd = ymin - 2*voff/3 + dy*(i_int=INTERPOLATE(iy-1, iy, 
                                        data[ix][iy-1],
                                        data[ix][iy], value));
                if (!(i_int<iy-1 || i_int>iy)) {
                    xd = xmin + (nx-1)*dx + hoff/2;
                    plot_string(&xd, &yd, string);
                    }
                }
            }
        }
}

void PlotShapesData(SHAPE_DATA *shape, long shapes, double xmin, double xmax, double ymin, double ymax) {
  long i, j;
  long origPen;
  origPen = set_linetype(0);
  /*set_mapping(xmin, xmax, ymin, ymax);*/
  for (i=0; i<shapes; i++) {
    for (j=0; j<shape[i].nPages; j++) {
      if (shape[i].plotSymbols==0)
        plot_lines(shape[i].xData[j], shape[i].yData[j], shape[i].nPoints[j],
                   shape[i].lineType, shape[i].thickness);
      else
        plot_points_fill(shape[i].xData[j], shape[i].yData[j], shape[i].nPoints[j],
                         shape[i].lineType, 0, shape[i].scale, shape[i].thickness, shape[i].fill);
    }
  }
  set_linetype(origPen);
}

#define SWAP(x, y, tmp) (tmp=(x),(x)=(y),(y)=tmp)

