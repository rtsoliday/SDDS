/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* prototypes for this file are in draw_contours.prot */
/* routine: draw_contours()
 * purpose: draw contour lines
 *
 * Original code: A.D.Cox, 1982.
 * C version: M.Borland, 1991.
 $Log: not supported by cvs2svn $
 Revision 1.3  2000/05/11 20:19:17  soliday
 Added extra argument to plot_lines.

 Revision 1.2  1995/09/05 21:08:46  saunders
 First test release of the SDDS1.5 package.

 */
#include "mdb.h"
#include "graph.h"
#include "contour.h"

void draw_contours(double **fxy, double xmin, double xmax, double ymin, double ymax,
                   long nx, long ny, double *cval, long nc, long thickness, long flags)
{
    double x[2], y[2], f[2][2], xx[4], yy[4];
    long ix, iy, ic;
    double fxymax, fxymin, value;
    double dx, dy;
    long nn;

    dx = (xmax-xmin)/(nx-1);
    dy = (ymax-ymin)/(ny-1);

    fxymin = fxymax = fxy[0][0];
    for (ix=0; ix<nx; ix++) {
        for (iy=0; iy<ny; iy++) {
            if ((value = fxy[ix][iy])>fxymax)
                fxymax = value;
            if (value<fxymin)
                fxymin = value;
            }
        }

    /* eliminate meaningless contours */
    for (ic=0; ic<nc; ic++) {
        if (cval[ic]<fxymin) {
            cval += 1;
            nc--;
            ic--;
            }
        else if (cval[ic]>fxymax) {
            nc = ic;
            break;
            }
        }

    /* sweep over region of plot and draw contours */

    for (ix=0; ix<nx-1; ix++) {
        x[1] = (x[0] = ix+1) + 1;
        for (iy=0; iy<ny-1; iy++) {
            y[1] = (y[0] = iy+1) + 1;
            if (flags&Y_FLIP) {
              f[0][0] = fxy[ix  ][ny-iy-1];
              f[0][1] = fxy[ix  ][ny-iy  ];
              f[1][0] = fxy[ix+1][ny-iy-1];
              f[1][1] = fxy[ix+1][ny-iy  ];
            } else {
              f[0][0] = fxy[ix  ][iy  ];
              f[0][1] = fxy[ix  ][iy+1];
              f[1][0] = fxy[ix+1][iy  ];
              f[1][1] = fxy[ix+1][iy+1];
            }
            for (ic=0; ic<nc; ic++) {
                nn = 0;
                if ((f[0][0]<=cval[ic] && f[1][0]>=cval[ic]) || 
                    (f[1][0]<=cval[ic] && f[0][0]>=cval[ic])    )
                    find_intersection(x[0],y[0],f[0][0],f[1][0],cval[ic],xx,yy,&nn);
                if ((f[0][0]<cval[ic] && f[0][1]>=cval[ic]) || 
                    (f[0][1]<=cval[ic] && f[0][0]>cval[ic])    )
                    find_intersection(y[0],x[0],f[0][0],f[0][1],cval[ic],yy,xx,&nn);
                if ((f[0][1]<=cval[ic] && f[1][1]>=cval[ic]) || 
                    (f[1][1]<=cval[ic] && f[0][1]>=cval[ic])    )
                    find_intersection(x[0],y[1],f[0][1],f[1][1],cval[ic],xx,yy,&nn);
                if ((f[1][0]<=cval[ic] && f[1][1]>cval[ic]) || 
                    (f[1][1]<cval[ic] && f[1][0]>=cval[ic])    )
                    find_intersection(y[0],x[1],f[1][0],f[1][1],cval[ic],yy,xx,&nn);
                while (nn>=2) {
                    nn -= 2;
                    xx[nn]   = (xx[nn  ]-1)*dx + xmin;
                    xx[nn+1] = (xx[nn+1]-1)*dx + xmin;
                    yy[nn]   = (yy[nn  ]-1)*dy + ymin;
                    yy[nn+1] = (yy[nn+1]-1)*dy + ymin;
                    plot_lines(xx+nn, yy+nn, 2, PRESET_LINETYPE,thickness);
                    }
                }
            }
        }
    }

void find_intersection(double x, double y, double f1, double f2, double cval,
    double xx[4], double yy[4], long *nn)
{
    yy[*nn] = y;
    if (f1==f2)
        xx[*nn] = x + 0.5;
    else
        xx[*nn] = x + (cval-f1)/(f2-f1); 
    (*nn) += 1;
    }

