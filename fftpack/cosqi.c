/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* cosqi.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:12:57  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int cosqi_(integer *n, doublereal *wsave)
{
    /* Initialized data */

    doublereal pih = 1.57079632679491;

    /* System generated locals */
    integer i_1;

    /* Builtin functions */
    double cos(doublereal);

    /* Local variables */
    integer k;
    extern /* Subroutine */ int rffti_(integer *, doublereal *);
    doublereal fk, dt;

    /* Parameter adjustments */
    --wsave;

    /* Function Body */
    dt = pih / (doublereal) (*n);
    fk = 0.;
    i_1 = *n;
    for (k = 1; k <= i_1; ++k) {
	fk += 1.;
	wsave[k] = cos(fk * dt);
/* L101: */
    }
    rffti_(n, &wsave[*n + 1]);
    return 0;
} /* cosqi_ */


