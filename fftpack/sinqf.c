/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* sinqf.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:16  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int sinqf_(integer *n, doublereal *x, doublereal *wsave)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    integer k;
    extern /* Subroutine */ int cosqf_(integer *, doublereal *, doublereal *);

    doublereal xhold;
    integer kc, ns2;

    /* Parameter adjustments */
    --x;
    --wsave;

    /* Function Body */
    if (*n == 1) {
	return 0;
    }
    ns2 = *n / 2;
    i_1 = ns2;
    for (k = 1; k <= i_1; ++k) {
	kc = *n - k;
	xhold = x[k];
	x[k] = x[kc + 1];
	x[kc + 1] = xhold;
/* L101: */
    }
    cosqf_(n, &x[1], &wsave[1]);
    i_1 = *n;
    for (k = 2; k <= i_1; k += 2) {
	x[k] = -x[k];
/* L102: */
    }
    return 0;
} /* sinqf_ */


