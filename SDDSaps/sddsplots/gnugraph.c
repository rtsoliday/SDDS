/* GNUPLOT - graphics.c */
/*
 * Copyright (C) 1986, 1987, 1990   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the modified code.  Modifications are to be distributed 
 * as patches to released version.
 *  
 * This software  is provided "as is" without express or implied warranty.
 * 
 *
 * AUTHORS
 * 
 *   Original Software:
 *     Thomas Williams,  Colin Kelley.
 * 
 *   Gnuplot 2.0 additions:
 *       Russell Lang, Dave Kotz, John Campbell.
 * 
 * send your comments or suggestions to (pixar!info-gnuplot@sun.com).
 * 
 */

#define UNDEFINED 0
#define INRANGE   1
#define OUTRANGE  2

#ifndef max             /* Lattice C has max() in math.h, but shouldn't! */
#define max(a,b) ((a) > (b) ? a : b)
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? a : b)
#endif


void plot_lines(double *xd, double *yd, long n, int line_type, int line_thickness)
{
    int i;                              /* point index */
    int x,y;                            /* point in terminal coordinates */
    struct termentry *t = &term_tbl[term];
    int prev = UNDEFINED; /* type of previous point */
    double ex, ey;                      /* an edge point */
    double lx[2], ly[2];                /* two edge points */
    check_scales("plot_lines");
    line_type = set_linetype(line_type);
    set_linethickness(line_thickness);
    for (i = 0; i < n; i++) {
        x = map_x(xd[i]);
        y = map_y(yd[i]);
        if ((!clip_lines1 && !clip_lines2) || (inrange(xd[i], xmin, xmax) && inrange(yd[i], ymin, ymax))) {
            if (prev == INRANGE) {
                (*t->vector)(x,y);
                } 
            else if (prev == OUTRANGE) {
                /* from outrange to inrange */
                if (!clip_lines1) {
                    (*t->move)(x,y);
                    } 
                else {
                    edge_intersect(xd[i-1], yd[i-1], xd[i], yd[i], &ex, &ey);
                    (*t->move)(map_x(ex), map_y(ey));
                    (*t->vector)(x,y);
                    }
                } 
            else {  /* prev == UNDEFINED */
                (*t->move)(x,y);
                (*t->vector)(x,y);
                }
            prev = INRANGE;
            }
        else {
            if (prev == INRANGE) {
                /* from inrange to outrange */
                if (clip_lines1) {
                    edge_intersect(xd[i-1], yd[i-1], xd[i], yd[i], &ex, &ey);
                    (*t->vector)(map_x(ex), map_y(ey));
                    }
                } 
            else if (prev == OUTRANGE) {
                /* from outrange to outrange */
                if (clip_lines2) {
                    if (two_edge_intersect(xd[i-1], yd[i-1], xd[i], yd[i], lx, ly)) {
                        (*t->move)(map_x(lx[0]), map_y(ly[0]));
                        (*t->vector)(map_x(lx[1]), map_y(ly[1]));
                        }
                    }
                }
            prev = OUTRANGE;
            }
        }
    xu_pos = xd[n-1];
    yu_pos = yd[n-1];
    set_linetype(line_type);
    }


/* single edge intersection algorithm */
/* Given two points, one inside and one outside the plot, return
 * the point where an edge of the plot intersects the line segment defined 
 * by the two points.
 */

void edge_intersect(
        double ax, double ay,
        double bx, double by,
        double *ex, double *ey      /* the point where it crosses an edge */
        )
{
    double x, y;                        /* possible intersection point */

    if (by == ay) {
           /* horizontal line */
           /* assume inrange(by, ymin, ymax) */
           *ey = by;            /* == ay */

           if (inrange(xmax, ax, bx))
                *ex = xmax;
           else if (inrange(xmin, ax, bx))
                *ex = xmin;
           else {
                (*term_tbl[term].text)();
            (void) fflush(outfile);
                bomb("in edge_intersect", NULL);
           }
           return;
    } else if (bx == ax) {
           /* vertical line */
           /* assume inrange(bx, xmin, xmax) */
           *ex = bx;            /* == ax */

           if (inrange(ymax, ay, by))
                *ey = ymax;
           else if (inrange(ymin, ay, by))
                *ey = ymin;
           else {
                (*term_tbl[term].text)();
/*          (void) fflush(outfile); */
                bomb("in edge_intersect", NULL);
           }
           return;
    }

    /* slanted line of some kind */

    /* does it intersect ymin edge */
    if (inrange(ymin, ay, by) && ymin != ay && ymin != by) {
           x = ax + (ymin-ay) * ((bx-ax) / (by-ay));
           if (inrange(x, xmin, xmax)) {
                  *ex = x;
                  *ey = ymin;
                  return;                       /* yes */
           }
    }
    
    /* does it intersect ymax edge */
    if (inrange(ymax, ay, by) && ymax != ay && ymax != by) {
           x = ax + (ymax-ay) * ((bx-ax) / (by-ay));
           if (inrange(x, xmin, xmax)) {
                  *ex = x;
                  *ey = ymax;
                  return;                       /* yes */
           }
    }

    /* does it intersect xmin edge */
    if (inrange(xmin, ax, bx) && xmin != ax && xmin != bx) {
           y = ay + (xmin-ax) * ((by-ay) / (bx-ax));
           if (inrange(y, ymin, ymax)) {
                  *ex = xmin;
                  *ey = y;
                  return;
           }
    }

    /* does it intersect xmax edge */
    if (inrange(xmax, ax, bx) && xmax != ax && xmax != bx) {
           y = ay + (xmax-ax) * ((by-ay) / (bx-ax));
           if (inrange(y, ymin, ymax)) {
                  *ex = xmax;
                  *ey = y;
                  return;
           }
    }

    /* It is possible for one or two of the [ab][xy] values to be -VERYLARGE.
        * If ax=bx=-VERYLARGE or ay=by=-VERYLARGE we have already returned 
        * FALSE above. Otherwise we fall through all the tests above. 
        * If two are -VERYLARGE, it is ax=ay=-VERYLARGE or bx=by=-VERYLARGE 
        * since either a or b must be INRANGE. 
        * Note that for ax=ay=-VERYLARGE or bx=by=-VERYLARGE we can do nothing.
        * Handle them carefully here. As yet we have no way for them to be 
        * +VERYLARGE.
        */
    if (ax == -VERYLARGE) {
           if (ay != -VERYLARGE) {
                  *ex = min(xmin, xmax);
                  *ey = by;
                  return;
           }
    } else if (bx == -VERYLARGE) {
           if (by != -VERYLARGE) {
                  *ex = min(xmin, xmax);
                  *ey = ay;
                  return;
           }
    } else if (ay == -VERYLARGE) {
           /* note we know ax != -VERYLARGE */
           *ex = bx;
           *ey = min(ymin, ymax);
           return;
    } else if (by == -VERYLARGE) {
           /* note we know bx != -VERYLARGE */
           *ex = ax;
           *ey = min(ymin, ymax);
           return;
    }

    /* If we reach here, then either one point is (-VERYLARGE,-VERYLARGE), 
        * or the inrange point is on the edge, and
     * the line segment from the outrange point does not cross any 
        * other edges to get there. In either case, we return the inrange 
        * point as the 'edge' intersection point. This will basically draw
        * line.
        */
    if (inrange(bx,xmin,xmax) && inrange(by,ymin,ymax)) {
           *ex = bx; 
           *ey = by;
    } else {
           *ex = ax; 
           *ey = ay;
    }
    return;
}

/* double edge intersection algorithm */
/* Given two points, both outside the plot, return
 * the points where an edge of the plot intersects the line segment defined 
 * by the two points. There may be zero, one, two, or an infinite number
 * of intersection points. (One means an intersection at a corner, infinite
 * means overlaying the edge itself). We return FALSE when there is nothing
 * to draw (zero intersections), and TRUE when there is something to 
 * draw (the one-point case is a degenerate of the two-point case and we do 
 * not distinguish it - we draw it anyway).
 */

int                             /* any intersection? */
two_edge_intersect(
        double ax, double ay,    /* starting point */
        double bx, double by,    /* ending point */
        double *lx, double *ly   /* intersection point */
        )
{
    double x, y;                        /* possible intersection point */
    int intersect = FALSE;

    if (by == ay) {
           /* horizontal line */
           /* y coord must be in range, and line must span both xmin and xmax */
           /* note that spanning xmin implies spanning xmax */
           if (inrange(by, ymin, ymax) && inrange(xmin, ax, bx)) {
                  *lx++ = xmin;
                  *ly++ = by;
                  *lx++ = xmax;
                  *ly++ = by;
                  return(TRUE);
           } else
                return(FALSE);
    } else if (bx == ax) {
           /* vertical line */
           /* x coord must be in range, and line must span both ymin and ymax */
           /* note that spanning ymin implies spanning ymax */
           if (inrange(bx, xmin, xmax) && inrange(ymin, ay, by)) {
                  *lx++ = bx;
                  *ly++ = ymin;
                  *lx++ = bx;
                  *ly++ = ymax;
                  return(TRUE);
           } else
                return(FALSE);
    }

    /* slanted line of some kind */
    /* there can be only zero or two intersections below */

    /* does it intersect ymin edge */
    if (inrange(ymin, ay, by)) {
           x = ax + (ymin-ay) * ((bx-ax) / (by-ay));
           if (inrange(x, xmin, xmax)) {
                  *lx++ = x;
                  *ly++ = ymin;
                  intersect = TRUE;
           }
    }
    
    /* does it intersect ymax edge */
    if (inrange(ymax, ay, by)) {
           x = ax + (ymax-ay) * ((bx-ax) / (by-ay));
           if (inrange(x, xmin, xmax)) {
                  *lx++ = x;
                  *ly++ = ymax;
                  intersect = TRUE;
           }
    }

    /* does it intersect xmin edge */
    if (inrange(xmin, ax, bx)) {
           y = ay + (xmin-ax) * ((by-ay) / (bx-ax));
           if (inrange(y, ymin, ymax)) {
                  *lx++ = xmin;
                  *ly++ = y;
                  intersect = TRUE;
           }
    }

    /* does it intersect xmax edge */
    if (inrange(xmax, ax, bx)) {
           y = ay + (xmax-ax) * ((by-ay) / (bx-ax));
           if (inrange(y, ymin, ymax)) {
                  *lx++ = xmax;
                  *ly++ = y;
                  intersect = TRUE;
           }
    }

    if (intersect)
         return(TRUE);

    /* It is possible for one or more of the [ab][xy] values to be -VERYLARGE.
        * If ax=bx=-VERYLARGE or ay=by=-VERYLARGE we have already returned
        * FALSE above.
        * Note that for ax=ay=-VERYLARGE or bx=by=-VERYLARGE we can do nothing.
        * Otherwise we fall through all the tests above. 
        * Handle them carefully here. As yet we have no way for them to be +VERYLARGE.
        */
    if (ax == -VERYLARGE) {
           if (ay != -VERYLARGE
                  && inrange(by, ymin, ymax) && inrange(xmax, ax, bx)) {
                  *lx++ = xmin;
                  *ly = by;
                  *lx++ = xmax;
                  *ly = by;
                  intersect = TRUE;
           }
    } else if (bx == -VERYLARGE) {
           if (by != -VERYLARGE
                  && inrange(ay, ymin, ymax) && inrange(xmax, ax, bx)) {
                  *lx++ = xmin;
                  *ly = ay;
                  *lx++ = xmax;
                  *ly = ay;
                  intersect = TRUE;
           }
    } else if (ay == -VERYLARGE) {
           /* note we know ax != -VERYLARGE */
           if (inrange(bx, xmin, xmax) && inrange(ymax, ay, by)) {
                  *lx++ = bx;
                  *ly = ymin;
                  *lx++ = bx;
                  *ly = ymax;
                  intersect = TRUE;
           }
    } else if (by == -VERYLARGE) {
           /* note we know bx != -VERYLARGE */
           if (inrange(ax, xmin, xmax) && inrange(ymax, ay, by)) {
                  *lx++ = ax;
                  *ly = ymin;
                  *lx++ = ax;
                  *ly = ymax;
                  intersect = TRUE;
           }
    }

    return(intersect);
}


