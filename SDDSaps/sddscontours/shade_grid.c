/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

  /* file: shade_grid
   * contents: go_shade_grid()
   *
   * Michael Borland, 1993
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

void go_shade_grid(
		   char *device, char *title, char *xvar, char *yvar, char *topline,
		   double **data, double xmin, double xmax, double ymin, double ymax,
		   double *xintervals, double *yintervals, long nx, long ny,
		   double min_level, double max_level, long n_levels, 
		   double hue0, double hue1, long layout[2], long ix, long iy,                  
		   char *shapes, int *pen, long flags, long pause_interval,
		   long thickness, unsigned long long tsetFlags, char *colorName, 
                   char *colorUnits, double xlabelScale, double ylabelScale, long gray, 
                   long fill_screen, short xlog, long nx_offset, short show_gaps)
{
  long reverse, pen0;
  double map[4], average, spread;
  double pmin, pmax, qmin, qmax;
  double wpmin, wpmax, wqmin, wqmax;
  long title_at_top=0;
  char newLabel[50];
  time_t timeValue;

  reverse = 0;
  if (n_levels<0) {
    reverse = 1;
    n_levels *= -1;
  }
    
  if (!(flags&DEVICE_DEFINED)) {
    change_term(device, strlen(device));
  }
  if ((ix == 0) && (iy == 0))
    graphics_on();
  if(gray) 
    alloc_spectrum(n_levels+1,0,0,0,0,65535,65535,65535);
  else   
    alloc_spectrum(n_levels+1,4,0,0,0,0,0,0);

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
  if (flags&TITLE_AT_TOP) {
    /* adjust pspace to make room at top for the title */
    get_pspace(&pmin, &pmax, &qmin, &qmax);
    spread = qmax-qmin;
    qmax -= 0.04*spread;
    qmin -= 0.04*spread;
    set_pspace(pmin, pmax, qmin, qmax);
    title_at_top = 1;
  }
  if (fill_screen)
    set_pspace(0, 1, 0, 1);
  set_clipping(1, 1, 1);

  if (flags&EQUAL_ASPECT1) 
    set_aspect(1.0L);
  else if (flags&EQUAL_ASPECT_1)
    set_aspect(-1.0L);

  pen0 = set_linetype(0);
  set_linetype(pen0);
  set_linethickness(thickness);

  set_linetype(pen[0]);
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
      if (!(flags&NO_XSCALES)) {
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
          make_scales_with_label(0, xlog, 1, 0.0, 0.02, 0.0, 
                                 0.0, 1.0, 0, 0, 
                                 thickness, thickness, 0, 
                                 thickness, xvar, 0, thickness, xlabelScale);
        }
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
                                 thickness, yvar,0 ,thickness, ylabelScale);
        }
      }
    }
  }
  if (!(flags&NO_LABELS)) {
    plotTitle(title, 1, title_at_top, 1.0, 0.0, thickness, 0);
    plotTitle(topline, 0, 0, 1.0, 0.0, thickness, 0);
    
   /* if (flags&NO_XSCALES) 
      plot_xlabel(xvar);
    if (flags&NO_YSCALES) 
      plot_ylabel(yvar); */
    
  }
  shade_grid(data, xmin, xmax, ymin, ymax, xintervals, yintervals, nx, ny, &min_level, &max_level, 
	     hue0, hue1, n_levels, reverse, flags, nx_offset, show_gaps);
  set_linetype(pen0);
  if (!(flags&NO_COLOR_BAR))
    make_intensity_bar(n_levels, 0, reverse, min_level, max_level, hue0, hue1, colorName, colorUnits, thickness, 1.0, 1.0, 0.0);
  set_linetype(pen[0]);

  if (flags&DATE_STAMP)
    time_date_stamp();

}
    

void shade_grid(double **fxy, double xmin, double xmax, double ymin, double ymax, 
		double *xintervals, double *yintervals, long nx, long ny, 
                double *min, double *max, double hue0, double hue1, long nlev, 
                long reverse, long flags, long nx_offset, short show_gaps)
{
  double xl, xh, yl, yh;
  long ix, ix1, ix2, iy, iy1, iy2;
  long xeq, yeq;
  double fxymax, fxymin, value;
  int shade, **sxy;
  double minXinterval=99e99, minYinterval=99e99;
  if (nlev && *max != *min) {
    fxymin = *min;
    fxymax = *max;
  }
  else {
    fxymin = fxymax = fxy[0][0];
    for (ix=0; ix<nx; ix++) {
      for (iy=0; iy<ny; iy++) {
	if ((value = fxy[ix][iy])>fxymax)
	  fxymax = value;
	if (value<fxymin)
	  fxymin = value;
      }
    }
    *min = fxymin;
    *max = fxymax;
    if (!nlev)
      nlev = 100;
  }
  if (flags&UNSUPPRESS_Y_ZERO) {
    if (fxymin>0)
      fxymin = 0;
    if (fxymax<0)
      fxymax = 0;
  }
  /*
    dx = (xmax-xmin)/(nx-1);
    dy = (ymax-ymin)/(ny-1);
  */
  sxy = (int**)zarray_2d(sizeof(**sxy), nx, ny);

  /* sweep over data and compute integer shade values */
  if (flags&Y_FLIP) {
    for (ix=0; ix<nx; ix++) {
      for (iy=0; iy<ny; iy++) {
        if (fxy[ix][ny-iy-1]<*min || fxy[ix][ny-iy-1]>*max) {
          sxy[ix][iy] = -1;
          continue;
        }
        if (isnan(fxy[ix][ny-iy-1])) {
          sxy[ix][iy]=-1;
        } else {
          if (reverse) {
            shade = nlev*(fxymax - fxy[ix][ny-iy-1])/(fxymax-fxymin);
          } else {
            shade = nlev*(fxy[ix][ny-iy-1] - fxymin)/(fxymax-fxymin);
          }
          if (nlev >= 100) {
            sxy[ix][iy] = (hue1-hue0)*(100*shade/nlev)+nlev*hue0;
          } else {
            sxy[ix][iy] = (hue1-hue0)*shade+nlev*hue0;
          }
        }
      }
    }
  } else {
    for (ix=0; ix<nx; ix++) {
      for (iy=0; iy<ny; iy++) {
        if (fxy[ix][iy]<*min || fxy[ix][iy]>*max) {
          sxy[ix][iy] = -1;
          continue;
        }
        if (isnan(fxy[ix][iy])) {
          sxy[ix][iy]=-1;
        } else {
          if (reverse) {
            shade = nlev*(fxymax - fxy[ix][iy])/(fxymax-fxymin);
          } else {
            shade = nlev*(fxy[ix][iy] - fxymin)/(fxymax-fxymin);
          }
          if (nlev >= 100) {
            sxy[ix][iy] = (hue1-hue0)*(100*shade/nlev)+nlev*hue0;
          } else {
            sxy[ix][iy] = (hue1-hue0)*shade+nlev*hue0;
          }
        }
      }
    }
  }

  /* figure out whether it is better to band by x or y */
  xeq = yeq = 0;
  for (ix=0; ix<nx-1; ix++) {
    for (iy=0; iy<ny-1; iy++) {
      if (sxy[ix][iy]==sxy[ix+1][iy])
	xeq++;
      if (sxy[ix][iy]==sxy[ix][iy+1])
	yeq++;
    }
  }

  if (show_gaps) {
    /* calc min intervals */
    for (ix=1; ix<nx; ix++) {
      if ((xintervals[ix+nx_offset] - xintervals[ix+nx_offset - 1]) < minXinterval) {
        minXinterval = (xintervals[ix+nx_offset] - xintervals[ix+nx_offset - 1]);
      }
    }
    for (iy=1; iy<ny; iy++) {
      if ((yintervals[iy] - yintervals[iy - 1]) < minYinterval) {
        minYinterval = (xintervals[iy] - xintervals[iy-1]);
      }
    }
  }

  /* do shading */
  if (xeq<yeq) {
    for (ix=0; ix<nx; ix++) {
      if (ix == 0) {
	xl = xintervals[ix+nx_offset];
      } else {
        if (show_gaps)
          xl = xintervals[ix+nx_offset] - (minXinterval / 2.0);
        else
          xl = xintervals[ix+nx_offset] - ((xintervals[ix+nx_offset] - xintervals[ix+nx_offset - 1]) / 2.0);
      }
      if (ix == nx - 1) {
	xh = xintervals[ix+nx_offset];
      } else {
        if (show_gaps)
          xh = xintervals[ix+nx_offset] + (minXinterval / 2.0);
        else
          xh = xintervals[ix+nx_offset] + ((xintervals[ix+nx_offset + 1] - xintervals[ix+nx_offset]) / 2.0);
      }
      if ((xh<xmin) || (xl>xmax)) {
        continue;
      }
      if (xl<xmin) {
        xl = xmin;
      }
      if (xh>xmax) {
        xh = xmax;
      }
      iy1 = 0;
      iy2 = 1;
      while (iy2<=ny) {
        if (sxy[ix][iy1] == -1) {
          iy1 = iy2;
          iy2 += 1;
          continue;
        }
        if (iy1 == 0) {
          yl = yintervals[iy1];
        } else {
          if (show_gaps)
            yl = yintervals[iy1] - (minYinterval / 2.0);
          else
            yl = yintervals[iy1] - ((yintervals[iy1] - yintervals[iy1 - 1]) / 2.0);
        }
        if (iy1 == ny - 1) {
          yh = yintervals[iy1];
        } else {
          if (show_gaps)
            yh = yintervals[iy1] + (minYinterval / 2.0);
          else
            yh = yintervals[iy1] + ((yintervals[iy1 + 1] - yintervals[iy1]) / 2.0);
        }
        if (yl<ymin)
          yl = ymin;
        if (yh>ymax)
          yh = ymax;
        shade_box(sxy[ix][iy1], xl, xh, yl, yh);
        iy1 = iy2;
        iy2 += 1;
      }
    }
  } else {
    for (iy=0; iy<ny; iy++) {
      if (iy == 0) {
	yl = yintervals[iy];
      } else {
        if (show_gaps)
          yl = yintervals[iy] - (minYinterval / 2.0);
        else
          yl = yintervals[iy] - ((yintervals[iy] - yintervals[iy - 1]) / 2.0);
      }
      if (iy == ny - 1) {
	yh = yintervals[iy];
      } else {
        if (show_gaps)
          yh = yintervals[iy] + (minYinterval / 2.0);
        else
          yh = yintervals[iy] + ((yintervals[iy + 1] - yintervals[iy]) / 2.0);
      }
      if (yl<ymin)
	yl = ymin;
      if (yh>ymax)
	yh = ymax;
      ix1 = 0;
      ix2 = 1;
      while (ix2<=nx) {
	if (sxy[ix1][iy] == -1) {
	  ix1 = ix2;
	  ix2 += 1;
	  continue;
	}
	if (ix1 == 0) {
	  xl = xintervals[ix1+nx_offset];
	} else {
          if (show_gaps)
            xl = xintervals[ix1+nx_offset] - (minXinterval / 2.0);
          else
            xl = xintervals[ix1+nx_offset] - ((xintervals[ix1+nx_offset] - xintervals[ix1+nx_offset - 1]) / 2.0);
	}
	if (ix1 == nx - 1) {
	  xh = xintervals[ix1+nx_offset];
	} else {
          if (show_gaps)
            xh = xintervals[ix1+nx_offset] + (minXinterval / 2.0);
          else
            xh = xintervals[ix1+nx_offset] + ((xintervals[ix1+nx_offset + 1] - xintervals[ix1+nx_offset]) / 2.0);
	}
        if ((xh<xmin) || (xl>xmax)) {
          ix1 = ix2;
          ix2 += 1;
          continue;
        }
	if (xl<xmin)
	  xl = xmin;
	if (xh>xmax)
	  xh = xmax;
	shade_box(sxy[ix1][iy], xl, xh, yl, yh);
	ix1 = ix2;
	ix2 += 1;
      }
    }
  }
  for (ix=0; ix<nx; ix++)
    if (sxy[ix]) free(sxy[ix]);
  if (sxy) free(sxy);
}


