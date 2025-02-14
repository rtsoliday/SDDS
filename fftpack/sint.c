/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* sint.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:19  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int sint_(integer *n, doublereal *x, doublereal *wsave)
{
    extern /* Subroutine */ int sint1_(integer *, doublereal *, doublereal *, 
	    doublereal *, doublereal *, doublereal *);
    integer np1, iw1, iw2, iw3;

    /* Parameter adjustments */
    --x;
    --wsave;

    /* Function Body */
    np1 = *n + 1;
    iw1 = *n / 2 + 1;
    iw2 = iw1 + np1;
    iw3 = iw2 + np1;
    sint1_(n, &x[1], &wsave[1], &wsave[iw1], &wsave[iw2], &wsave[iw3]);
    return 0;
} /* sint_ */


