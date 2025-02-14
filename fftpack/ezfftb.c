/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* ezfftb.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:08  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int ezfftb_(integer *n, doublereal *r, doublereal *azero, 
	doublereal *a, doublereal *b, doublereal *wsave)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    integer i;
    extern /* Subroutine */ int rfftb_(integer *, doublereal *, doublereal *);

    integer ns2;

    /* Parameter adjustments */
    --r;
    --a;
    --b;
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
    r[1] = *azero;
    return 0;
L102:
    r[1] = *azero + a[1];
    r[2] = *azero - a[1];
    return 0;
L103:
    ns2 = (*n - 1) / 2;
    i_1 = ns2;
    for (i = 1; i <= i_1; ++i) {
	r[i * 2] = a[i] * .5;
	r[(i << 1) + 1] = b[i] * -.5;
/* L104: */
    }
    r[1] = *azero;
    if (*n % 2 == 0) {
	r[*n] = a[ns2 + 1];
    }
    rfftb_(n, &r[1], &wsave[*n + 1]);
    return 0;
} /* ezfftb_ */


