/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* costi.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:01  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int costi_(integer *n, doublereal *wsave)
{
    /* Initialized data */

    doublereal pi = 3.14159265358979;

    /* System generated locals */
    integer i_1;

    /* Builtin functions */
    double sin(doublereal), cos(doublereal);

    /* Local variables */
    integer k;
    extern /* Subroutine */ int rffti_(integer *, doublereal *);
    integer kc;
    doublereal fk, dt;
    integer nm1, np1, ns2;

    /* Parameter adjustments */
    --wsave;

    /* Function Body */
    if (*n <= 3) {
	return 0;
    }
    nm1 = *n - 1;
    np1 = *n + 1;
    ns2 = *n / 2;
    dt = pi / (doublereal) nm1;
    fk = 0.;
    i_1 = ns2;
    for (k = 2; k <= i_1; ++k) {
	kc = np1 - k;
	fk += 1.;
	wsave[k] = sin(fk * dt) * 2.;
	wsave[kc] = cos(fk * dt) * 2.;
/* L101: */
    }
    rffti_(&nm1, &wsave[*n + 1]);
    return 0;
} /* costi_ */


