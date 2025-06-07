/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: plot_data_sets()
 * purpose: plots multiple data sets on the same graph
 *      
 * Michael Borland, 1991-994.
 */
#include "mdb.h"
#include "graph.h"

void time_date_stamp()
{
    double h_save, v_save, hu, vu;
    double xmin, xmax, ymin, ymax;
    double x[2], y[2], yrange, xrange;
    double pmin, pmax, qmin, qmax;
    double posc;
    char *ptr0, *ptr1;

    get_char_size(&h_save, &v_save, 1);   /* save character size in user's coordinates */
    hu = h_save; vu = v_save;
    get_mapping(&xmin, &xmax, &ymin, &ymax);
    get_pspace(&pmin, &pmax,&qmin, &qmax);
    if (!(yrange=ymax-ymin))
        bomb("y range is zero (time_date_stamp)", NULL);
    if (!(xrange=xmax-xmin))
        bomb("x range is zero (time_date_stamp)", NULL);
    if (pmin==pmax || qmin==qmax)
        bomb("pspace invalid", NULL);

    hu = (xmax-xmin)*0.01;
    vu = hu*v_save/h_save;
    set_char_size(hu, vu, 1);
    posc = 1 - (1-pmax)/2;
    set_clipping(0, 0, 0);
    ptr0 = mtimes();
    if ((ptr1=strrchr(ptr0, ' '))) {
        *ptr1++ = 0;
        x[0] = (posc-pmin)*xrange/(pmax-pmin)+xmin;
        y[0] = ymax + 2.0*vu;
        cplot_string(x, y, ptr0);
        x[0] = (posc-pmin)*xrange/(pmax-pmin)+xmin;
        y[0] = ymax + 3.75*vu;
        cplot_string(x, y, ptr1);
        }
    free(ptr0);
    set_char_size(h_save, v_save, 1);
    set_clipping(1,1,1);
    }

