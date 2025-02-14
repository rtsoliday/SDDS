/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radbg.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:51  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int radbg_(integer *ido, integer *ip, integer *l1, integer *
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
    idp2 = *ido + 2;
    nbd = (*ido - 1) / 2;
    ipp2 = *ip + 2;
    ipph = (*ip + 1) / 2;
    if (*ido < *l1) {
	goto L103;
    }
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	i_2 = *ido;
	for (i = 1; i <= i_2; ++i) {
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + (k * cc_dim2 + 1) * 
		    cc_dim1];
/* L101: */
	}
/* L102: */
    }
    goto L106;
L103:
    i_1 = *ido;
    for (i = 1; i <= i_1; ++i) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + (k * cc_dim2 + 1) * 
		    cc_dim1];
/* L104: */
	}
/* L105: */
    }
L106:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	j2 = j + j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    ch[(k + j * ch_dim2) * ch_dim1 + 1] = cc[*ido + (j2 - 2 + k * 
		    cc_dim2) * cc_dim1] + cc[*ido + (j2 - 2 + k * cc_dim2) * 
		    cc_dim1];
	    ch[(k + jc * ch_dim2) * ch_dim1 + 1] = cc[(j2 - 1 + k * cc_dim2) *
		     cc_dim1 + 1] + cc[(j2 - 1 + k * cc_dim2) * cc_dim1 + 1];
/* L107: */
	}
/* L108: */
    }
    if (*ido == 1) {
	goto L116;
    }
    if (nbd < *l1) {
	goto L112;
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    i_3 = *ido;
	    for (i = 3; i <= i_3; i += 2) {
		ic = idp2 - i;
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = cc[i - 1 + ((j << 1)
			 - 1 + k * cc_dim2) * cc_dim1] + cc[ic - 1 + ((j << 1)
			 - 2 + k * cc_dim2) * cc_dim1];
		ch[i - 1 + (k + jc * ch_dim2) * ch_dim1] = cc[i - 1 + ((j << 
			1) - 1 + k * cc_dim2) * cc_dim1] - cc[ic - 1 + ((j << 
			1) - 2 + k * cc_dim2) * cc_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = cc[i + ((j << 1) - 1 + 
			k * cc_dim2) * cc_dim1] - cc[ic + ((j << 1) - 2 + k * 
			cc_dim2) * cc_dim1];
		ch[i + (k + jc * ch_dim2) * ch_dim1] = cc[i + ((j << 1) - 1 + 
			k * cc_dim2) * cc_dim1] + cc[ic + ((j << 1) - 2 + k * 
			cc_dim2) * cc_dim1];
/* L109: */
	    }
/* L110: */
	}
/* L111: */
    }
    goto L116;
L112:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    ic = idp2 - i;
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = cc[i - 1 + ((j << 1)
			 - 1 + k * cc_dim2) * cc_dim1] + cc[ic - 1 + ((j << 1)
			 - 2 + k * cc_dim2) * cc_dim1];
		ch[i - 1 + (k + jc * ch_dim2) * ch_dim1] = cc[i - 1 + ((j << 
			1) - 1 + k * cc_dim2) * cc_dim1] - cc[ic - 1 + ((j << 
			1) - 2 + k * cc_dim2) * cc_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = cc[i + ((j << 1) - 1 + 
			k * cc_dim2) * cc_dim1] - cc[ic + ((j << 1) - 2 + k * 
			cc_dim2) * cc_dim1];
		ch[i + (k + jc * ch_dim2) * ch_dim1] = cc[i + ((j << 1) - 1 + 
			k * cc_dim2) * cc_dim1] + cc[ic + ((j << 1) - 2 + k * 
			cc_dim2) * cc_dim1];
/* L113: */
	    }
/* L114: */
	}
/* L115: */
    }
L116:
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
	    c2[ik + l * c2_dim1] = ch2[ik + ch2_dim1] + ar1 * ch2[ik + (
		    ch2_dim1 << 1)];
	    c2[ik + lc * c2_dim1] = ai1 * ch2[ik + *ip * ch2_dim1];
/* L117: */
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
		c2[ik + l * c2_dim1] += ar2 * ch2[ik + j * ch2_dim1];
		c2[ik + lc * c2_dim1] += ai2 * ch2[ik + jc * ch2_dim1];
/* L118: */
	    }
/* L119: */
	}
/* L120: */
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *idl1;
	for (ik = 1; ik <= i_2; ++ik) {
	    ch2[ik + ch2_dim1] += ch2[ik + j * ch2_dim1];
/* L121: */
	}
/* L122: */
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    ch[(k + j * ch_dim2) * ch_dim1 + 1] = c1[(k + j * c1_dim2) * 
		    c1_dim1 + 1] - c1[(k + jc * c1_dim2) * c1_dim1 + 1];
	    ch[(k + jc * ch_dim2) * ch_dim1 + 1] = c1[(k + j * c1_dim2) * 
		    c1_dim1 + 1] + c1[(k + jc * c1_dim2) * c1_dim1 + 1];
/* L123: */
	}
/* L124: */
    }
    if (*ido == 1) {
	goto L132;
    }
    if (nbd < *l1) {
	goto L128;
    }
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    i_3 = *ido;
	    for (i = 3; i <= i_3; i += 2) {
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = c1[i - 1 + (k + j * 
			c1_dim2) * c1_dim1] - c1[i + (k + jc * c1_dim2) * 
			c1_dim1];
		ch[i - 1 + (k + jc * ch_dim2) * ch_dim1] = c1[i - 1 + (k + j *
			 c1_dim2) * c1_dim1] + c1[i + (k + jc * c1_dim2) * 
			c1_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = c1[i + (k + j * c1_dim2)
			 * c1_dim1] + c1[i - 1 + (k + jc * c1_dim2) * c1_dim1]
			;
		ch[i + (k + jc * ch_dim2) * ch_dim1] = c1[i + (k + j * 
			c1_dim2) * c1_dim1] - c1[i - 1 + (k + jc * c1_dim2) * 
			c1_dim1];
/* L125: */
	    }
/* L126: */
	}
/* L127: */
    }
    goto L132;
L128:
    i_1 = ipph;
    for (j = 2; j <= i_1; ++j) {
	jc = ipp2 - j;
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    i_3 = *l1;
	    for (k = 1; k <= i_3; ++k) {
		ch[i - 1 + (k + j * ch_dim2) * ch_dim1] = c1[i - 1 + (k + j * 
			c1_dim2) * c1_dim1] - c1[i + (k + jc * c1_dim2) * 
			c1_dim1];
		ch[i - 1 + (k + jc * ch_dim2) * ch_dim1] = c1[i - 1 + (k + j *
			 c1_dim2) * c1_dim1] + c1[i + (k + jc * c1_dim2) * 
			c1_dim1];
		ch[i + (k + j * ch_dim2) * ch_dim1] = c1[i + (k + j * c1_dim2)
			 * c1_dim1] + c1[i - 1 + (k + jc * c1_dim2) * c1_dim1]
			;
		ch[i + (k + jc * ch_dim2) * ch_dim1] = c1[i + (k + j * 
			c1_dim2) * c1_dim1] - c1[i - 1 + (k + jc * c1_dim2) * 
			c1_dim1];
/* L129: */
	    }
/* L130: */
	}
/* L131: */
    }
L132:
    if (*ido == 1) {
	return 0;
    }
    i_1 = *idl1;
    for (ik = 1; ik <= i_1; ++ik) {
	c2[ik + c2_dim1] = ch2[ik + ch2_dim1];
/* L133: */
    }
    i_1 = *ip;
    for (j = 2; j <= i_1; ++j) {
	i_2 = *l1;
	for (k = 1; k <= i_2; ++k) {
	    c1[(k + j * c1_dim2) * c1_dim1 + 1] = ch[(k + j * ch_dim2) * 
		    ch_dim1 + 1];
/* L134: */
	}
/* L135: */
    }
    if (nbd > *l1) {
	goto L139;
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
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i 
			- 1 + (k + j * ch_dim2) * ch_dim1] - wa[idij] * ch[i 
			+ (k + j * ch_dim2) * ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i + (
			k + j * ch_dim2) * ch_dim1] + wa[idij] * ch[i - 1 + (
			k + j * ch_dim2) * ch_dim1];
/* L136: */
	    }
/* L137: */
	}
/* L138: */
    }
    goto L143;
L139:
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
		c1[i - 1 + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i 
			- 1 + (k + j * ch_dim2) * ch_dim1] - wa[idij] * ch[i 
			+ (k + j * ch_dim2) * ch_dim1];
		c1[i + (k + j * c1_dim2) * c1_dim1] = wa[idij - 1] * ch[i + (
			k + j * ch_dim2) * ch_dim1] + wa[idij] * ch[i - 1 + (
			k + j * ch_dim2) * ch_dim1];
/* L140: */
	    }
/* L141: */
	}
/* L142: */
    }
L143:
    return 0;
} /* radbg_ */


