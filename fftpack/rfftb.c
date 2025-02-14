/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* rfftb.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:03  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int rfftb_(integer *n, doublereal *r, doublereal *wsave)
{
    extern /* Subroutine */ int rfftb1_(integer *, doublereal *, doublereal *,
	     doublereal *, doublereal *);

    /* Parameter adjustments */
    --r;
    --wsave;

    /* Function Body */
    if (*n == 1) {
	return 0;
    }
    rfftb1_(n, &r[1], &wsave[1], &wsave[*n + 1], &wsave[(*n << 1) + 1]);
    return 0;
} /* rfftb_ */


