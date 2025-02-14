/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* sint1.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:21  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int sint1_(integer *n, doublereal *war, doublereal *was, 
	doublereal *xh, doublereal *x, integer *ifac)
{
    /* Initialized data */

    doublereal sqrt3 = 1.73205080756888;

    /* System generated locals */
    integer i_1;

    /* Local variables */
    integer modn, i, k;
    doublereal xhold, t1, t2;
    extern /* Subroutine */ int rfftf1_(integer *, doublereal *, doublereal *,
	     doublereal *, integer *);
    integer kc, np1, ns2;

    /* Parameter adjustments */
    --war;
    --was;
    --xh;
    --x;
    --ifac;

    /* Function Body */
    i_1 = *n;
    for (i = 1; i <= i_1; ++i) {
	xh[i] = war[i];
	war[i] = x[i];
/* L100: */
    }
    if ((i_1 = *n - 2) < 0) {
	goto L101;
    } else if (i_1 == 0) {
	goto L102;
    } else {
	goto L103;
    }
L101:
    xh[1] += xh[1];
    goto L106;
L102:
    xhold = sqrt3 * (xh[1] + xh[2]);
    xh[2] = sqrt3 * (xh[1] - xh[2]);
    xh[1] = xhold;
    goto L106;
L103:
    np1 = *n + 1;
    ns2 = *n / 2;
    x[1] = 0.;
    i_1 = ns2;
    for (k = 1; k <= i_1; ++k) {
	kc = np1 - k;
	t1 = xh[k] - xh[kc];
	t2 = was[k] * (xh[k] + xh[kc]);
	x[k + 1] = t1 + t2;
	x[kc + 1] = t2 - t1;
/* L104: */
    }
    modn = *n % 2;
    if (modn != 0) {
	x[ns2 + 2] = xh[ns2 + 1] * 4.;
    }
    rfftf1_(&np1, &x[1], &xh[1], &war[1], &ifac[1]);
    xh[1] = x[1] * .5;
    i_1 = *n;
    for (i = 3; i <= i_1; i += 2) {
	xh[i - 1] = -x[i];
	xh[i] = xh[i - 2] + x[i - 1];
/* L105: */
    }
    if (modn != 0) {
	goto L106;
    }
    xh[*n] = -x[*n + 1];
L106:
    i_1 = *n;
    for (i = 1; i <= i_1; ++i) {
	x[i] = war[i];
	war[i] = xh[i];
/* L107: */
    }
    return 0;
} /* sint1_ */


