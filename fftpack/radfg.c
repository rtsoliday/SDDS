/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radfg.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:01  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int radfg_(integer *ido, integer *ip, integer *l1, integer *
	idl1, doublereal *cc, doublereal *c1, doublereal *c2, doublereal *ch, 
	doublereal *ch2, doublereal *wa)
{
    /* Initialized data */

    doublereal tpi = 6.28318530717959;

    /* System generated locals */
    integer ch_dim1, ch_dim2, ch_offset, cc_dim1, cc_dim2, cc_offset, c1_dim1,
	     c1_dim2, c1_offset, c2_dim1, c2_offset, ch2_dim1, ch2_offset, 
	    i_1, i_2, i_3;

    /* Builtin functions */
    double cos(doublereal), sin(doublereal);

    /* Local variables */
    integer idij, ipph, i, j, k, l, j2, ic, jc, lc, ik, is;
    doublereal dc2, ai1, ai2, ar1, ar2, ds2;
    integer nbd;
    doublereal dcp, arg, dsp, ar1h, ar2h;
    integer idp2, ipp2;

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
    arg = tpi / (doublereal) (*ip);
    dcp = cos(arg);
    dsp = sin(arg);
    ipph = (*ip + 1) / 2;
    ipp2 = *ip + 2;
    idp2 = *ido + 2;
    nbd = (*ido - 1) / 2;
    if (*ido == 1) {
	goto L119;
    }
    i_1 = *idl1;
    for (ik = 1; ik <= i_1; ++ik) {
	ch2[ik + ch2_dim1] = c2[ik + c2_dim1];
/* L101: */
    }
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    ch[(k + j * ch_dim2) * ch_dim1 + 1] = c1[(k + j * c1_dim2) * 
		    c1_dim1 + 1];
/* L102: */
	}
/* L103: */
    }
    if (nbd > *l1) {
	goto L107;
    }
    is = -(*ido);
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	is += *ido;
	idij = is;
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    idij += 2;
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = wa[idij - 1] * c1[i 
			- 1 + (k + j * c1_dim2) * c1_dim1] + wa[idij] * c1[i 
			+ (k + j * c1_dim2) * c1_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = wa[idij - 1] * c1[i + (
			k + j * c1_dim2) * c1_dim1] - wa[idij] * c1[i - 1 + (
			k + j * c1_dim2) * c1_dim1];
/* L104: */
	    }
/* L105: */
	}
/* L106: */
    }
    goto L111;
L107:
    is = -(*ido);
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	is += *ido;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    idij = is;
	    i_3 = *ido;
	    for (i = 3; i <= i_3; i += 2) {
		idij += 2;
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = wa[idij - 1] * c1[i 
			- 1 + (k + j * c1_dim2) * c1_dim1] + wa[idij] * c1[i 
			+ (k + j * c1_dim2) * c1_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = wa[idij - 1] * c1[i + (
			k + j * c1_dim2) * c1_dim1] - wa[idij] * c1[i - 1 + (
			k + j * c1_dim2) * c1_dim1];
/* L108: */
	    }
/* L109: */
	}
/* L110: */
    }
L111:
    if (nbd < *l1) {
	goto L115;
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    i_3 = *ido;
	    for (i = 3; i <= i_3; i += 2) {
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = ch[i - 1 + (k + j * 
			ch_dim2) * ch_dim1] + ch[i - 1 + (k + jc * ch_dim2) * 
			ch_dim1];
		c1[i - 1 + (k + jc * c1_dim2) * c1_dim1] = ch[i + (k + j * 
			ch_dim2) * ch_dim1] - ch[i + (k + jc * ch_dim2) * 
			ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = ch[i + (k + j * ch_dim2)
			 * ch_dim1] + ch[i + (k + jc * ch_dim2) * ch_dim1];
		c1[i + (k + jc * c1_dim2) * c1_dim1] = ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1] - ch[i - 1 + (k + j * ch_dim2) * 
			ch_dim1];
/* L112: */
	    }
/* L113: */
	}
/* L114: */
    }
    goto L121;
L115:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = ch[i - 1 + (k + j * 
			ch_dim2) * ch_dim1] + ch[i - 1 + (k + jc * ch_dim2) * 
			ch_dim1];
		c1[i - 1 + (k + jc * c1_dim2) * c1_dim1] = ch[i + (k + j * 
			ch_dim2) * ch_dim1] - ch[i + (k + jc * ch_dim2) * 
			ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = ch[i + (k + j * ch_dim2)
			 * ch_dim1] + ch[i + (k + jc * ch_dim2) * ch_dim1];
		c1[i + (k + jc * c1_dim2) * c1_dim1] = ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1] - ch[i - 1 + (k + j * ch_dim2) * 
			ch_dim1];
/* L116: */
	    }
/* L117: */
	}
/* L118: */
    }
    goto L121;
L119:
    i_1 = *idl1;
    for (ik = 1; ik <= i_1; ++ik) {
	c2[ik + c2_dim1] = ch2[ik + ch2_dim1];
/* L120: */
    }
L121:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    c1[(k + j * c1_dim2) * c1_dim1 + 1] = ch[(k + j * ch_dim2) * 
		    ch_dim1 + 1] + ch[(k + jc * ch_dim2) * ch_dim1 + 1];
	    c1[(k + jc * c1_dim2) * c1_dim1 + 1] = ch[(k + jc * ch_dim2) * 
		    ch_dim1 + 1] - ch[(k + j * ch_dim2) * ch_dim1 + 1];
/* L122: */
	}
/* L123: */
    }

    ar1 = 1.;
    ai1 = 0.;
    i_1 = ipph;
    for (l = 2; l <= i_1; ++l) {
	lc = ipp2 - l;
	ar1h = dcp * ar1 - dsp * ai1;
	ai1 = dcp * ai1 + dsp * ar1;
	ar1 = ar1h;
	i_2 = *idl1;
	for (ik = 1; ik <= i_2; ++ik) {
	    ch2[ik + l * ch2_dim1] = c2[ik + c2_dim1] + ar1 * c2[ik + (
		    c2_dim1 << 1)];
	    ch2[ik + lc * ch2_dim1] = ai1 * c2[ik + *ip * c2_dim1];
/* L124: */
	}
	dc2 = ar1;
	ds2 = ai1;
	ar2 = ar1;
	ai2 = ai1;
	i_2 = ipph;
	for (j = 3; j <= i_2; ++j) {
	    jc = ipp2 - j;
	    ar2h = dc2 * ar2 - ds2 * ai2;
	    ai2 = dc2 * ai2 + ds2 * ar2;
	    ar2 = ar2h;
	    i_3 = *idl1;
	    for (ik = 1; ik <= i_3; ++ik) {
		ch2[ik + l * ch2_dim1] += ar2 * c2[ik + j * c2_dim1];
		ch2[ik + lc * ch2_dim1] += ai2 * c2[ik + jc * c2_dim1];
/* L125: */
	    }
/* L126: */
	}
/* L127: */
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *idl1;
	for (ik = 1; ik <= i_2; ++ik) {
	    ch2[ik + ch2_dim1] += c2[ik + j * c2_dim1];
/* L128: */
	}
/* L129: */
    }

    if (*ido < *l1) {
	goto L132;
    }
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	i_2 = *ido;
	for (i = 1; i <= i_2; ++i) {
	    cc[i + (k * cc_dim2 + 1) * cc_dim1] = ch[i + (k + ch_dim2) * 
		    ch_dim1];
/* L130: */
	}
/* L131: */
    }
    goto L135;
L132:
    i_1 = *ido;
    for (i = 1; i <= i_1; ++i) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    cc[i + (k * cc_dim2 + 1) * cc_dim1] = ch[i + (k + ch_dim2) * 
		    ch_dim1];
/* L133: */
	}
/* L134: */
    }
L135:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	j2 = j + j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    cc[*ido + (j2 - 2 + k * cc_dim2) * cc_dim1] = ch[(k + j * ch_dim2)
		     * ch_dim1 + 1];
	    cc[(j2 - 1 + k * cc_dim2) * cc_dim1 + 1] = ch[(k + jc * ch_dim2) *
		     ch_dim1 + 1];
/* L136: */
	}
/* L137: */
    }
    if (*ido == 1) {
	return 0;
    }
    if (nbd < *l1) {
	goto L141;
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	j2 = j + j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    i_3 = *ido;
	    for (i = 3; i <= i_3; i += 2) {
		ic = idp2 - i;
		cc[i - 1 + (j2 - 1 + k * cc_dim2) * cc_dim1] = ch[i - 1 + (k 
			+ j * ch_dim2) * ch_dim1] + ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1];
		cc[ic - 1 + (j2 - 2 + k * cc_dim2) * cc_dim1] = ch[i - 1 + (k 
			+ j * ch_dim2) * ch_dim1] - ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1];
		cc[i + (j2 - 1 + k * cc_dim2) * cc_dim1] = ch[i + (k + j * 
			ch_dim2) * ch_dim1] + ch[i + (k + jc * ch_dim2) * 
			ch_dim1];
		cc[ic + (j2 - 2 + k * cc_dim2) * cc_dim1] = ch[i + (k + jc * 
			ch_dim2) * ch_dim1] - ch[i + (k + j * ch_dim2) * 
			ch_dim1];
/* L138: */
	    }
/* L139: */
	}
/* L140: */
    }
    return 0;
L141:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	j2 = j + j;
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    ic = idp2 - i;
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		cc[i - 1 + (j2 - 1 + k * cc_dim2) * cc_dim1] = ch[i - 1 + (k 
			+ j * ch_dim2) * ch_dim1] + ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1];
		cc[ic - 1 + (j2 - 2 + k * cc_dim2) * cc_dim1] = ch[i - 1 + (k 
			+ j * ch_dim2) * ch_dim1] - ch[i - 1 + (k + jc * 
			ch_dim2) * ch_dim1];
		cc[i + (j2 - 1 + k * cc_dim2) * cc_dim1] = ch[i + (k + j * 
			ch_dim2) * ch_dim1] + ch[i + (k + jc * ch_dim2) * 
			ch_dim1];
		cc[ic + (j2 - 2 + k * cc_dim2) * cc_dim1] = ch[i + (k + jc * 
			ch_dim2) * ch_dim1] - ch[i + (k + j * ch_dim2) * 
			ch_dim1];
/* L142: */
	    }
/* L143: */
	}
/* L144: */
    }
    return 0;
} /* radfg_ */


