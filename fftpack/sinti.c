/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* sinti.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)

*/

#include "f2c.h"

/* Subroutine */ int sinti_(integer *n, doublereal *wsave)
{
    /* Initialized data */

    doublereal pi = 3.14159265358979;

    /* System generated locals */
    integer i_1;

    /* Builtin functions */
    double sin(doublereal);

    /* Local variables */
    integer k;
    extern /* Subroutine */ int rffti_(integer *, doublereal *);
    doublereal dt;
    integer np1, ns2;

    /* Parameter adjustments */
    --wsave;

    /* Function Body */
    if (*n <= 1) {
	return 0;
    }
    ns2 = *n / 2;
    np1 = *n + 1;
    dt = pi / (doublereal) np1;
    i_1 = ns2;
    for (k = 1; k <= i_1; ++k) {
	wsave[k] = sin(k * dt) * 2.;
/* L101: */
    }
    rffti_(&np1, &wsave[ns2 + 1]);
    return 0;
} /* sinti_ */


