/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* ezfftf.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:10  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int ezfftf_(integer *n, doublereal *r, doublereal *azero, 
	doublereal *a, doublereal *b, doublereal *wsave)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    integer i;
    extern /* Subroutine */ int rfftf_(integer *, doublereal *, doublereal *);

    doublereal cf;
    integer ns2;
    doublereal cfm;
    integer ns2m;

    /* Parameter adjustments */
    --r;
    --a;
    --b;
    --wsave;

    /* Function Body */

/*                       VERSION 3  JUNE 1979 */

    if ((i_1 = *n - 2) < 0) {
	goto L101;
    } else if (i_1 == 0) {
	goto L102;
    } else {
	goto L103;
    }
L101:
    *azero = r[1];
    return 0;
L102:
    *azero = (r[1] + r[2]) * .5;
    a[1] = (r[1] - r[2]) * .5;
    return 0;
L103:
    i_1 = *n;
    for (i = 1; i <= i_1; ++i) {
	wsave[i] = r[i];
/* L104: */
    }
    rfftf_(n, &wsave[1], &wsave[*n + 1]);
    cf = 2. / (doublereal) (*n);
    cfm = -cf;
    *azero = cf * .5 * wsave[1];
    ns2 = (*n + 1) / 2;
    ns2m = ns2 - 1;
    i_1 = ns2m;
    for (i = 1; i <= i_1; ++i) {
	a[i] = cf * wsave[i * 2];
	b[i] = cfm * wsave[(i << 1) + 1];
/* L105: */
    }
    if (*n % 2 == 1) {
	return 0;
    }
    a[ns2] = cf * .5 * wsave[*n];
    b[ns2] = 0.;
    return 0;
} /* ezfftf_ */


