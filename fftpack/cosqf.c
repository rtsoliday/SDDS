/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* cosqf.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:12:52  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int cosqf_(integer *n, doublereal *x, doublereal *wsave)
{
    /* Initialized data */

    doublereal sqrt2 = 1.4142135623731;

    /* System generated locals */
    integer i_1;

    /* Local variables */
    doublereal tsqx;
    extern /* Subroutine */ int cosqf1_(integer *, doublereal *, doublereal *,
	     doublereal *);

    /* Parameter adjustments */
    --x;
    --wsave;

    /* Function Body */
    if ((i_1 = *n - 2) < 0) {
	goto L102;
    } else if (i_1 == 0) {
	goto L101;
    } else {
	goto L103;
    }
L101:
    tsqx = sqrt2 * x[2];
    x[2] = x[1] - tsqx;
    x[1] += tsqx;
L102:
    return 0;
L103:
    cosqf1_(n, &x[1], &wsave[1], &wsave[*n + 1]);
    return 0;
} /* cosqf_ */


