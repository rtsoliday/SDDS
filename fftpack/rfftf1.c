/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* rfftf1.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:08  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int rfftf1_(integer *n, doublereal *c, doublereal *ch, 
	doublereal *wa, integer *ifac)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    extern /* Subroutine */ int radf2_(integer *, integer *, doublereal *, 
	    doublereal *, doublereal *), radf3_(integer *, integer *, 
	    doublereal *, doublereal *, doublereal *, doublereal *), radf4_(
	    integer *, integer *, doublereal *, doublereal *, doublereal *, 
	    doublereal *, doublereal *), radf5_(integer *, integer *, 
	    doublereal *, doublereal *, doublereal *, doublereal *, 
	    doublereal *, doublereal *);
    integer i;
    extern /* Subroutine */ int radfg_(integer *, integer *, integer *, 
	    integer *, doublereal *, doublereal *, doublereal *, doublereal *,
	     doublereal *, doublereal *);
    integer k1, l1, l2, na, kh, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    /* Parameter adjustments */
    --c;
    --ch;
    --wa;
    --ifac;

    /* Function Body */
    nf = ifac[2];
    na = 1;
    l2 = *n;
    iw = *n;
    i_1 = nf;
    for (k1 = 1; k1 <= i_1; ++k1) {
	kh = nf - k1;
	ip = ifac[kh + 3];
	l1 = l2 / ip;
	ido = *n / l2;
	idl1 = ido * l1;
	iw -= (ip - 1) * ido;
	na = 1 - na;
	if (ip != 4) {
	    goto L102;
	}
	ix2 = iw + ido;
	ix3 = ix2 + ido;
	if (na != 0) {
	    goto L101;
	}
	radf4_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2], &wa[ix3]);
	goto L110;
L101:
	radf4_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2], &wa[ix3]);
	goto L110;
L102:
	if (ip != 2) {
	    goto L104;
	}
	if (na != 0) {
	    goto L103;
	}
	radf2_(&ido, &l1, &c[1], &ch[1], &wa[iw]);
	goto L110;
L103:
	radf2_(&ido, &l1, &ch[1], &c[1], &wa[iw]);
	goto L110;
L104:
	if (ip != 3) {
	    goto L106;
	}
	ix2 = iw + ido;
	if (na != 0) {
	    goto L105;
	}
	radf3_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2]);
	goto L110;
L105:
	radf3_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2]);
	goto L110;
L106:
	if (ip != 5) {
	    goto L108;
	}
	ix2 = iw + ido;
	ix3 = ix2 + ido;
	ix4 = ix3 + ido;
	if (na != 0) {
	    goto L107;
	}
	radf5_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]
		);
	goto L110;
L107:
	radf5_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]
		);
	goto L110;
L108:
	if (ido == 1) {
	    na = 1 - na;
	}
	if (na != 0) {
	    goto L109;
	}
	radfg_(&ido, &ip, &l1, &idl1, &c[1], &c[1], &c[1], &ch[1], &ch[1], &
		wa[iw]);
	na = 1;
	goto L110;
L109:
	radfg_(&ido, &ip, &l1, &idl1, &ch[1], &ch[1], &ch[1], &c[1], &c[1], &
		wa[iw]);
	na = 0;
L110:
	l2 = l1;
/* L111: */
    }
    if (na == 1) {
	return 0;
    }
    i_1 = *n;
    for (i = 1; i <= i_1; ++i) {
	c[i] = ch[i];
/* L112: */
    }
    return 0;
} /* rfftf1_ */


