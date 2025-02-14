/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* cosqb.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:12:47  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int cosqb_(integer *n, doublereal *x, doublereal *wsave)
{
    /* Initialized data */

    doublereal tsqrt2 = 2.82842712474619;

    /* System generated locals */
    integer i_1;

    /* Local variables */
    doublereal x1;
    extern /* Subroutine */ int cosqb1_(integer *, doublereal *, doublereal *,
	     doublereal *);

    /* Parameter adjustments */
    --x;
    --wsave;

    /* Function Body */
    if ((i_1 = *n - 2) < 0) {
	goto L101;
    } else if (i_1 == 0) {
	goto L102;
    } else {
	goto L103;
    }
L101:
    x[1] *= 4.;
    return 0;
L102:
    x1 = (x[1] + x[2]) * 4.;
    x[2] = tsqrt2 * (x[1] - x[2]);
    x[1] = x1;
    return 0;
L103:
    cosqb1_(n, &x[1], &wsave[1], &wsave[*n + 1]);
    return 0;
} /* cosqb_ */


