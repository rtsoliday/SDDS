/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: arrows.c
 * purpose: support for arrows in mpl 
 * Michael Borland, 1992
 */
#include <assert.h>
#include <ctype.h>
#include "mdb.h"
#include "graphics.h"
#include "graph.h"


static double arrow_scale_factor = 1;
static double arrow_barb_angle = 30./180.*PI;
static double arrow_barb_length = -1;

void set_arrow_scale_factor(double *asf)
{
    double tmp;

    tmp = arrow_scale_factor;
    arrow_scale_factor = *asf;
    *asf = tmp;
    }

void set_arrow_barb_length(double *asf)
{
    double tmp;

    tmp = arrow_barb_length;
    arrow_barb_length = *asf;
    *asf = tmp;
    }

void set_arrow_barb_angle(double *asf)
{
    double tmp;

    tmp = arrow_barb_angle;
    arrow_barb_angle = *asf;
    *asf = tmp;
    }

void plot_arrow(double x, double y, double length, double angle,
    double barb_length, double barb_angle,
    long arrow_type, long arrow_flags, long thickness
    )
{
    static double xd[6], yd[6];
    long points;

    if (arrow_scale_factor>0)
        length *= arrow_scale_factor;
    else {
        length *= -arrow_scale_factor;   
        angle += PI;
        }

    if (length<=0) {
        plot_dots(&x, &y, 1, labs(arrow_type), 0);
        return;
        }
    if (barb_angle<0)
        barb_angle = arrow_barb_angle;
    if (barb_length<0) {
        if (arrow_barb_length<0) {
            /* make barb length 1% of maximum dimension of plot */
            double x_min, x_max, y_min, y_max;
            get_mapping(&x_min, &x_max, &y_min, &y_max);
            if ((x_max -= x_min)<(y_max -= y_min))
                x_max = y_max;
            barb_length = x_max/100;
            }
        else
            barb_length = arrow_barb_length;
        }
        
    if (arrow_flags&ARROW_CENTERED) {
        x -= length/2*cos(angle);
        y -= length/2*sin(angle);
        }
    
    xd[0] = x;
    yd[0] = y;
    xd[1] = xd[0] + length*cos(angle);
    yd[1] = yd[0] + length*sin(angle);
    xd[2] = xd[1] + barb_length*cos(angle+PI-barb_angle);
    yd[2] = yd[1] + barb_length*sin(angle+PI-barb_angle);
    points = 3;
    if (!(arrow_flags&ARROW_SINGLEBARB)) {
        xd[3] = xd[1];
        yd[3] = yd[1];
        xd[4] = xd[1] + barb_length*cos(angle+PI+barb_angle);
        yd[4] = yd[1] + barb_length*sin(angle+PI+barb_angle);
        xd[5] = xd[1];
        yd[5] = yd[1];
        points = 6;
        }
    plot_lines(xd, yd, points, labs(arrow_type), arrow_flags&ARROW_THICKNESS_GIVEN?thickness:0);
    }

void plot_arrows(double *x, double *y, double *x1, double *y1, long n,
                 ARROW_SETTINGS *arrow)
{
    long i;
    double length, angle = 0, barbLength, barbAngle, arrowType;
    
    barbAngle = arrow->flags&ARROW_BARBANGLE_GIVEN?arrow->barbAngle:(30.0*PI/180);
    barbLength = arrow->flags&ARROW_BARBLENGTH_GIVEN?arrow->barbLength:0.35;
    arrowType = arrow->flags&ARROW_LINETYPE_GIVEN?arrow->linetype:get_linetype();
    
    for (i=0; i<n; i++) {
        if (arrow->flags&ARROW_POLAR_DATA) {
            length = x1[i];
            angle  = y1[i];
            }
        else if (arrow->flags&ARROW_SCALAR_DATA)
            length = y1[i];
        else /*if (arrow->flags&ARROW_CARTESIAN_DATA)*/ {
            length = sqrt(sqr(x1[i])+sqr(y1[i]));
            if (x1[i])
                angle = atan2(y1[i], x1[i]);
            else
                angle = y1[i]>0? PI/2 : -PI/2;
            }
        if (arrow->flags&ARROW_SCALE_GIVEN)
            length *= arrow->scale;
        if (!(arrow->flags&ARROW_SCALAR_DATA))
            plot_arrow(x[i], y[i], length, angle, barbLength*length, barbAngle, 
                       arrowType, arrow->flags, arrow->thickness);
        else
            plot_scalar(x[i], y[i], length, arrowType, arrow->flags);
        }
    }


void plot_scalar(double x, double y, double length, long type, unsigned long flags)
{
    double xd[5], yd[5];

    type = labs(type);
    if (length>=0) {
        plot_dots(&x, &y, 1, type, 0);
        if (length>0) {
            xd[0] = x            ; yd[0] = y + length/2;
            xd[1] = x + length/2 ; yd[1] = y;
            xd[2] = x            ; yd[2] = y - length/2;
            xd[3] = x - length/2 ; yd[3] = y;
            xd[4] = xd[0]        ; yd[4] = yd[0];
            plot_lines(xd, yd, 5, type, 0);
            }
        }
    else {
        xd[0] = x; yd[0] = y + length/2;
        xd[1] = x; yd[1] = y - length/2;
        plot_lines(xd, yd, 2, type,0);
        xd[0] = x + length/2; yd[0] = y;
        xd[1] = x - length/2; yd[1] = y;
        plot_lines(xd, yd, 2, type,0);
        }
    }

void plot_arrow_old(double x, double y, double length, double angle,
    double barb_length, double barb_angle,
    long arrow_type, long arrow_flags
    )
{
    static double xd[6], yd[6];


    if (arrow_scale_factor>0)
        length *= arrow_scale_factor;
    else {
        length *= -arrow_scale_factor;   
        angle += PI;
        }

    if (length<=0) {
        plot_dots(&x, &y, 1, -1, 0);
        return;
        }
    if (barb_angle<0)
        barb_angle = arrow_barb_angle;
    if (barb_length<0) {
        if (arrow_barb_length<0) {
            /* make barb length 1% of maximum dimension of plot */
            double x_min, x_max, y_min, y_max;
            get_mapping(&x_min, &x_max, &y_min, &y_max);
            if ((x_max -= x_min)<(y_max -= y_min))
                x_max = y_max;
            barb_length = x_max/100;
            }
        else
            barb_length = arrow_barb_length;
        }
        
    if (arrow_flags&CENTER_ARROWS) {
        x -= length/2*cos(angle);
        y -= length/2*sin(angle);
        }
    
    xd[0] = x;
    yd[0] = y;
    xd[1] = xd[0] + length*cos(angle);
    yd[1] = yd[0] + length*sin(angle);
    xd[2] = xd[1] + barb_length*cos(angle+PI-barb_angle);
    yd[2] = yd[1] + barb_length*sin(angle+PI-barb_angle);
    xd[3] = xd[1];
    yd[3] = yd[1];
    xd[4] = xd[1] + barb_length*cos(angle+PI+barb_angle);
    yd[4] = yd[1] + barb_length*sin(angle+PI+barb_angle);
    xd[5] = xd[1];
    yd[5] = yd[1];
    plot_lines(xd, yd, 6, labs(arrow_type),0);
    }

void plot_arrows_old(double *x, double *y, double *length, double *angle, long n,
    double arrow_type, double arrow_code)
{
    long i;

    if (!length || !angle)
        bomb("length and/or angle arrays are NULL for arrow plotting", NULL);

    for (i=0; i<n; i++)
        plot_arrow(x[i], y[i], length[i], angle[i], -1.0L, -1.0L, arrow_type, arrow_code, 0);
    }



