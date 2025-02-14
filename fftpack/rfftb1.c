/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* rfftb1.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:05  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int rfftb1_(integer *n, doublereal *c, doublereal *ch, 
	doublereal *wa, integer *ifac)
{
    /* System generated locals */
    integer i_1;

    /* Local variables */
    extern /* Subroutine */ int radb2_(integer *, integer *, doublereal *, 
	    doublereal *, doublereal *), radb3_(integer *, integer *, 
	    doublereal *, doublereal *, doublereal *, doublereal *), radb4_(
	    integer *, integer *, doublereal *, doublereal *, doublereal *, 
	    doublereal *, doublereal *), radb5_(integer *, integer *, 
	    doublereal *, doublereal *, doublereal *, doublereal *, 
	    doublereal *, doublereal *);
    integer i;
    extern /* Subroutine */ int radbg_(integer *, integer *, integer *, 
	    integer *, doublereal *, doublereal *, doublereal *, doublereal *,
	     doublereal *, doublereal *);
    integer k1, l1, l2, na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    /* Parameter adjustments */
    --c;
    --ch;
    --wa;
    --ifac;

    /* Function Body */
    nf = ifac[2];
    na = 0;
    l1 = 1;
    iw = 1;
    i_1 = nf;
    for (k1 = 1; k1 <= i_1; ++k1) {
	ip = ifac[k1 + 2];
	l2 = ip * l1;
	ido = *n / l2;
	idl1 = ido * l1;
	if (ip != 4) {
	    goto L103;
	}
	ix2 = iw + ido;
	ix3 = ix2 + ido;
	if (na != 0) {
	    goto L101;
	}
	radb4_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2], &wa[ix3]);
	goto L102;
L101:
	radb4_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2], &wa[ix3]);
L102:
	na = 1 - na;
	goto L115;
L103:
	if (ip != 2) {
	    goto L106;
	}
	if (na != 0) {
	    goto L104;
	}
	radb2_(&ido, &l1, &c[1], &ch[1], &wa[iw]);
	goto L105;
L104:
	radb2_(&ido, &l1, &ch[1], &c[1], &wa[iw]);
L105:
	na = 1 - na;
	goto L115;
L106:
	if (ip != 3) {
	    goto L109;
	}
	ix2 = iw + ido;
	if (na != 0) {
	    goto L107;
	}
	radb3_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2]);
	goto L108;
L107:
	radb3_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2]);
L108:
	na = 1 - na;
	goto L115;
L109:
	if (ip != 5) {
	    goto L112;
	}
	ix2 = iw + ido;
	ix3 = ix2 + ido;
	ix4 = ix3 + ido;
	if (na != 0) {
	    goto L110;
	}
	radb5_(&ido, &l1, &c[1], &ch[1], &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]
		);
	goto L111;
L110:
	radb5_(&ido, &l1, &ch[1], &c[1], &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]
		);
L111:
	na = 1 - na;
	goto L115;
L112:
	if (na != 0) {
	    goto L113;
	}
	radbg_(&ido, &ip, &l1, &idl1, &c[1], &c[1], &c[1], &ch[1], &ch[1], &
		wa[iw]);
	goto L114;
L113:
	radbg_(&ido, &ip, &l1, &idl1, &ch[1], &ch[1], &ch[1], &c[1], &c[1], &
		wa[iw]);
L114:
	if (ido == 1) {
	    na = 1 - na;
	}
L115:
	l1 = l2;
	iw += (ip - 1) * ido;
/* L116: */
    }
    if (na == 0) {
	return 0;
    }
    i_1 = *n;
    for (i = 1; i <= i_1; ++i) {
	c[i] = ch[i];
/* L117: */
    }
    return 0;
} /* rfftb1_ */


