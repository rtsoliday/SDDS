/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* passf.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:32  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int passf_(integer *nac, integer *ido, integer *ip, integer *
	l1, integer *idl1, doublereal *cc, doublereal *c1, doublereal *c2, 
	doublereal *ch, doublereal *ch2, doublereal *wa)
{
    /* System generated locals */
    integer ch_dim1, ch_dim2, ch_offset, cc_dim1, cc_dim2, cc_offset, c1_dim1,
	     c1_dim2, c1_offset, c2_dim1, c2_offset, ch2_dim1, ch2_offset, 
	    i_1, i_2, i_3;

    /* Local variables */
    integer idij, idlj, idot, ipph, i, j, k, l, jc, lc, ik, idj, 
	    idl, inc, idp;
    doublereal wai, war;
    integer ipp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_dim2 = *ip;
    cc_offset = cc_dim1 * (cc_dim2 + 1) + 1;
    cc -= cc_offset;
    c1_dim1 = *ido;
    c1_dim2 = *l1;
    c1_offset = c1_dim1 * (c1_dim2 + 1) + 1;
    c1 -= c1_offset;
    c2_dim1 = *idl1;
    c2_offset = c2_dim1 + 1;
    c2 -= c2_offset;
    ch_dim1 = *ido;
    ch_dim2 = *l1;
    ch_offset = ch_dim1 * (ch_dim2 + 1) + 1;
    ch -= ch_offset;
    ch2_dim1 = *idl1;
    ch2_offset = ch2_dim1 + 1;
    ch2 -= ch2_offset;
    --wa;

    /* Function Body */
    idot = *ido / 2;
    ipp2 = *ip + 2;
    ipph = (*ip + 1) / 2;
    idp = *ip * *ido;

    if (*ido < *l1) {
	goto L106;
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    i_3 = *ido;
	    for (i = 1; i <= i_3; ++i) {
		ch[i + (k + j * ch_dim2) * ch_dim1] = cc[i + (j + k * cc_dim2)
			 * cc_dim1] + cc[i + (jc + k * cc_dim2) * cc_dim1];
		ch[i + (k + jc * ch_dim2) * ch_dim1] = cc[i + (j + k * 
			cc_dim2) * cc_dim1] - cc[i + (jc + k * cc_dim2) * 
			cc_dim1];
/* L101: */
	    }
/* L102: */
	}
/* L103: */
    }
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	i_2 = *ido;
	for (i = 1; i <= i_2; ++i) {
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + (k * cc_dim2 + 1) * 
		    cc_dim1];
/* L104: */
	}
/* L105: */
    }
    goto L112;
L106:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *ido;
	for (i = 1; i <= i_2; ++i) {
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		ch[i + (k + j * ch_dim2) * ch_dim1] = cc[i + (j + k * cc_dim2)
			 * cc_dim1] + cc[i + (jc + k * cc_dim2) * cc_dim1];
		ch[i + (k + jc * ch_dim2) * ch_dim1] = cc[i + (j + k * 
			cc_dim2) * cc_dim1] - cc[i + (jc + k * cc_dim2) * 
			cc_dim1];
/* L107: */
	    }
/* L108: */
	}
/* L109: */
    }
    i_1 = *ido;
    for (i = 1; i <= i_1; ++i) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + (k * cc_dim2 + 1) * 
		    cc_dim1];
/* L110: */
	}
/* L111: */
    }
L112:
    idl = 2 - *ido;
    inc = 0;
    i_1 = ipph;
    for (l = 2; l <= i_1; ++l) {
	lc = ipp2 - l;
	idl += *ido;
	i_2 = *idl1;
	for (ik = 1; ik <= i_2; ++ik) {
	    c2[ik + l * c2_dim1] = ch2[ik + ch2_dim1] + wa[idl - 1] * ch2[ik 
		    + (ch2_dim1 << 1)];
	    c2[ik + lc * c2_dim1] = -wa[idl] * ch2[ik + *ip * ch2_dim1];
/* L113: */
	}
	idlj = idl;
	inc += *ido;
	i_2 = ipph;
	for (j = 3; j <= i_2; ++j) {
	    jc = ipp2 - j;
	    idlj += inc;
	    if (idlj > idp) {
		idlj -= idp;
	    }
	    war = wa[idlj - 1];
	    wai = wa[idlj];
	    i_3 = *idl1;
	    for (ik = 1; ik <= i_3; ++ik) {
		c2[ik + l * c2_dim1] += war * ch2[ik + j * ch2_dim1];
		c2[ik + lc * c2_dim1] -= wai * ch2[ik + jc * ch2_dim1];
/* L114: */
	    }
/* L115: */
	}
/* L116: */
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *idl1;
	for (ik = 1; ik <= i_2; ++ik) {
	    ch2[ik + ch2_dim1] += ch2[ik + j * ch2_dim1];
/* L117: */
	}
/* L118: */
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *idl1;
	for (ik = 2; ik <= i_2; ik += 2) {
	    ch2[ik - 1 + j * ch2_dim1] = c2[ik - 1 + j * c2_dim1] - c2[ik + 
		    jc * c2_dim1];
	    ch2[ik - 1 + jc * ch2_dim1] = c2[ik - 1 + j * c2_dim1] + c2[ik + 
		    jc * c2_dim1];
	    ch2[ik + j * ch2_dim1] = c2[ik + j * c2_dim1] + c2[ik - 1 + jc * 
		    c2_dim1];
	    ch2[ik + jc * ch2_dim1] = c2[ik + j * c2_dim1] - c2[ik - 1 + jc * 
		    c2_dim1];
/* L119: */
	}
/* L120: */
    }
    *nac = 1;
    if (*ido == 2) {
	return 0;
    }
    *nac = 0;
    i_1 = *idl1;
    for (ik = 1; ik <= i_1; ++ik) {
	c2[ik + c2_dim1] = ch2[ik + ch2_dim1];
/* L121: */
    }
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    c1[(k + j * c1_dim2) * c1_dim1 + 1] = ch[(k + j * ch_dim2) * 
		    ch_dim1 + 1];
	    c1[(k + j * c1_dim2) * c1_dim1 + 2] = ch[(k + j * ch_dim2) * 
		    ch_dim1 + 2];
/* L122: */
	}
/* L123: */
    }
    if (idot > *l1) {
	goto L127;
    }
    idij = 0;
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	idij += 2;
	i_2 = *ido;
	for (i = 4; i <= i_2; i += 2) {
	    idij += 2;
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i 
			- 1 + (k + j * ch_dim2) * ch_dim1] + wa[idij] * ch[i 
			+ (k + j * ch_dim2) * ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i + (
			k + j * ch_dim2) * ch_dim1] - wa[idij] * ch[i - 1 + (
			k + j * ch_dim2) * ch_dim1];
/* L124: */
	    }
/* L125: */
	}
/* L126: */
    }
    return 0;
L127:
    idj = 2 - *ido;
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	idj += *ido;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    idij = idj;
	    i_3 = *ido;
	    for (i = 4; i <= i_3; i += 2) {
		idij += 2;
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i 
			- 1 + (k + j * ch_dim2) * ch_dim1] + wa[idij] * ch[i 
			+ (k + j * ch_dim2) * ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i + (
			k + j * ch_dim2) * ch_dim1] - wa[idij] * ch[i - 1 + (
			k + j * ch_dim2) * ch_dim1];
/* L128: */
	    }
/* L129: */
	}
/* L130: */
    }
    return 0;
} /* passf_ */


