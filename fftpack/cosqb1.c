/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* cosqb1.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:12:50  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int cosqb1_(integer *n, doublereal *x, doublereal *w, 
	doublereal *xh)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    integer modn, i, k;
    extern /* Subroutine */ int rfftb_(integer *, doublereal *, doublereal *);

    integer kc, np2, ns2;
    doublereal xim1;

    /* Parameter adjustments */
    --x;
    --w;
    --xh;

    /* Function Body */
    ns2 = (*n + 1) / 2;
    np2 = *n + 2;
    i_1 = *n;
    for (i = 3; i <= i_1; i += 2) {
	xim1 = x[i - 1] + x[i];
	x[i] -= x[i - 1];
	x[i - 1] = xim1;
/* L101: */
    }
    x[1] += x[1];
    modn = *n % 2;
    if (modn == 0) {
	x[*n] += x[*n];
    }
    rfftb_(n, &x[1], &xh[1]);
    i_1 = ns2;
    for (k = 2; k <= i_1; ++k) {
	kc = np2 - k;
	xh[k] = w[k - 1] * x[kc] + w[kc - 1] * x[k];
	xh[kc] = w[k - 1] * x[k] - w[kc - 1] * x[kc];
/* L102: */
    }
    if (modn == 0) {
	x[ns2 + 1] = w[ns2] * (x[ns2 + 1] + x[ns2 + 1]);
    }
    i_1 = ns2;
    for (k = 2; k <= i_1; ++k) {
	kc = np2 - k;
	x[k] = xh[k] + xh[kc];
	x[kc] = xh[k] - xh[kc];
/* L103: */
    }
    x[1] += x[1];
    return 0;
} /* cosqb1_ */


