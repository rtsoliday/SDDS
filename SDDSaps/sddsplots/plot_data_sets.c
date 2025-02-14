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
 $Log: not supported by cvs2svn $
 Revision 1.10  2009/09/23 21:45:40  borland
 Use mtimes() instead of mtime() for date/time stamp (includes seconds).

 Revision 1.9  2002/08/14 17:24:52  soliday
 Added Open License

 Revision 1.8  2000/06/02 20:56:53  soliday
 Added thickness option to lineTypeDefault, drawLine, axes, tick and subtick
 options.

 Revision 1.7  2000/04/27 19:29:27  soliday
 Added support for line thickness.

 Revision 1.6  1999/07/22 18:35:04  soliday
 Added WIN32 support

 Revision 1.5  1999/06/03 16:07:36  soliday
 Removed compiler warnings under linux.

 Revision 1.4  1998/08/25 15:05:28  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.3  1996/02/29  05:42:34  borland
 * Added variable dot size using subtype qualifier of -graphic option.
 * Required adding an extra argument to the plot_dots() routine.
 *
 * Revision 1.2  1995/09/05  21:09:31  saunders
 * First test release of the SDDS1.5 package.
 *
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

