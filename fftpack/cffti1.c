/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* cffti1.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)

*/

#include "f2c.h"

/* Subroutine */ int cffti1_(integer *n, doublereal *wa, integer *ifac)
{
    /* Initialized data */

    integer ntryh[4] = { 3,4,2,5 };

    /* System generated locals */
    integer i_1, i_2, i_3;

    /* Builtin functions */
    double cos(doublereal), sin(doublereal);

    /* Local variables */
    doublereal argh;
    integer idot, ntry=0, i, j;
    doublereal argld;
    integer i1, k1, l1, l2, ib;
    doublereal fi;
    integer ld, ii, nf, ip, nl, nq, nr;
    doublereal arg;
    integer ido, ipm;
    doublereal tpi;

    /* Parameter adjustments */
    --wa;
    --ifac;

    /* Function Body */
    nl = *n;
    nf = 0;
    j = 0;
L101:
    ++j;
    if (j - 4 <= 0) {
	goto L102;
    } else {
	goto L103;
    }
L102:
    ntry = ntryh[j - 1];
    goto L104;
L103:
    ntry += 2;
L104:
    nq = nl / ntry;
    nr = nl - ntry * nq;
    if (nr != 0) {
	goto L101;
    } else {
	goto L105;
    }
L105:
    ++nf;
    ifac[nf + 2] = ntry;
    nl = nq;
    if (ntry != 2) {
	goto L107;
    }
    if (nf == 1) {
	goto L107;
    }
    i_1 = nf;
    for (i = 2; i <= i_1; ++i) {
	ib = nf - i + 2;
	ifac[ib + 2] = ifac[ib + 1];
/* L106: */
    }
    ifac[3] = 2;
L107:
    if (nl != 1) {
	goto L104;
    }
    ifac[1] = *n;
    ifac[2] = nf;
    tpi = 6.28318530717959;
    argh = tpi / (doublereal) (*n);
    i = 2;
    l1 = 1;
    i_1 = nf;
    for (k1 = 1; k1 <= i_1; ++k1) {
	ip = ifac[k1 + 2];
	ld = 0;
	l2 = l1 * ip;
	ido = *n / l2;
	idot = ido + ido + 2;
	ipm = ip - 1;
	i_2 = ipm;
	for (j = 1; j <= i_2; ++j) {
	    i1 = i;
	    wa[i - 1] = 1.;
	    wa[i] = 0.;
	    ld += l1;
	    fi = 0.;
	    argld = (doublereal) ld * argh;
	    i_3 = idot;
	    for (ii = 4; ii <= i_3; ii += 2) {
		i += 2;
		fi += 1.;
		arg = fi * argld;
		wa[i - 1] = cos(arg);
		wa[i] = sin(arg);
/* L108: */
	    }
	    if (ip <= 5) {
		goto L109;
	    }
	    wa[i1 - 1] = wa[i - 1];
	    wa[i1] = wa[i];
L109:
	;}
	l1 = l2;
/* L110: */
    }
    return 0;
} /* cffti1_ */


