/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radf4.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)

*/

#include "f2c.h"

/* Subroutine */ int radf4_(integer *ido, integer *l1, doublereal *cc, 
	doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3)
{
    /* Initialized data */

    doublereal hsqt2 = .7071067811865475;

    /* System generated locals */
    integer cc_dim1, cc_dim2, cc_offset, ch_dim1, ch_offset, i_1, i_2;

    /* Local variables */
    integer i, k, ic;
    doublereal ci2, ci3, ci4, cr2, cr3, cr4, ti1, ti2, ti3, ti4, tr1, 
	    tr2, tr3, tr4;
    integer idp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_dim2 = *l1;
    cc_offset = cc_dim1 * (cc_dim2 + 1) + 1;
    cc -= cc_offset;
    ch_dim1 = *ido;
    ch_offset = ch_dim1 * 5 + 1;
    ch -= ch_offset;
    --wa1;
    --wa2;
    --wa3;

    /* Function Body */
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	tr1 = cc[(k + (cc_dim2 << 1)) * cc_dim1 + 1] + cc[(k + (cc_dim2 << 2))
		 * cc_dim1 + 1];
	tr2 = cc[(k + cc_dim2) * cc_dim1 + 1] + cc[(k + cc_dim2 * 3) * 
		cc_dim1 + 1];
	ch[((k << 2) + 1) * ch_dim1 + 1] = tr1 + tr2;
	ch[*ido + ((k << 2) + 4) * ch_dim1] = tr2 - tr1;
	ch[*ido + ((k << 2) + 2) * ch_dim1] = cc[(k + cc_dim2) * cc_dim1 + 1] 
		- cc[(k + cc_dim2 * 3) * cc_dim1 + 1];
	ch[((k << 2) + 3) * ch_dim1 + 1] = cc[(k + (cc_dim2 << 2)) * cc_dim1 
		+ 1] - cc[(k + (cc_dim2 << 1)) * cc_dim1 + 1];
/* L101: */
    }
    if ((i_1 = *ido - 2) < 0) {
	goto L107;
    } else if (i_1 == 0) {
	goto L105;
    } else {
	goto L102;
    }
L102:
    idp2 = *ido + 2;
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    ic = idp2 - i;
	    cr2 = wa1[i - 2] * cc[i - 1 + (k + (cc_dim2 << 1)) * cc_dim1] + 
		    wa1[i - 1] * cc[i + (k + (cc_dim2 << 1)) * cc_dim1];
	    ci2 = wa1[i - 2] * cc[i + (k + (cc_dim2 << 1)) * cc_dim1] - wa1[i 
		    - 1] * cc[i - 1 + (k + (cc_dim2 << 1)) * cc_dim1];
	    cr3 = wa2[i - 2] * cc[i - 1 + (k + cc_dim2 * 3) * cc_dim1] + wa2[
		    i - 1] * cc[i + (k + cc_dim2 * 3) * cc_dim1];
	    ci3 = wa2[i - 2] * cc[i + (k + cc_dim2 * 3) * cc_dim1] - wa2[i - 
		    1] * cc[i - 1 + (k + cc_dim2 * 3) * cc_dim1];
	    cr4 = wa3[i - 2] * cc[i - 1 + (k + (cc_dim2 << 2)) * cc_dim1] + 
		    wa3[i - 1] * cc[i + (k + (cc_dim2 << 2)) * cc_dim1];
	    ci4 = wa3[i - 2] * cc[i + (k + (cc_dim2 << 2)) * cc_dim1] - wa3[i 
		    - 1] * cc[i - 1 + (k + (cc_dim2 << 2)) * cc_dim1];
	    tr1 = cr2 + cr4;
	    tr4 = cr4 - cr2;
	    ti1 = ci2 + ci4;
	    ti4 = ci2 - ci4;
	    ti2 = cc[i + (k + cc_dim2) * cc_dim1] + ci3;
	    ti3 = cc[i + (k + cc_dim2) * cc_dim1] - ci3;
	    tr2 = cc[i - 1 + (k + cc_dim2) * cc_dim1] + cr3;
	    tr3 = cc[i - 1 + (k + cc_dim2) * cc_dim1] - cr3;
	    ch[i - 1 + ((k << 2) + 1) * ch_dim1] = tr1 + tr2;
	    ch[ic - 1 + ((k << 2) + 4) * ch_dim1] = tr2 - tr1;
	    ch[i + ((k << 2) + 1) * ch_dim1] = ti1 + ti2;
	    ch[ic + ((k << 2) + 4) * ch_dim1] = ti1 - ti2;
	    ch[i - 1 + ((k << 2) + 3) * ch_dim1] = ti4 + tr3;
	    ch[ic - 1 + ((k << 2) + 2) * ch_dim1] = tr3 - ti4;
	    ch[i + ((k << 2) + 3) * ch_dim1] = tr4 + ti3;
	    ch[ic + ((k << 2) + 2) * ch_dim1] = tr4 - ti3;
/* L103: */
	}
/* L104: */
    }
    if (*ido % 2 == 1) {
	return 0;
    }
L105:
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	ti1 = -hsqt2 * (cc[*ido + (k + (cc_dim2 << 1)) * cc_dim1] + cc[*ido + 
		(k + (cc_dim2 << 2)) * cc_dim1]);
	tr1 = hsqt2 * (cc[*ido + (k + (cc_dim2 << 1)) * cc_dim1] - cc[*ido + (
		k + (cc_dim2 << 2)) * cc_dim1]);
	ch[*ido + ((k << 2) + 1) * ch_dim1] = tr1 + cc[*ido + (k + cc_dim2) * 
		cc_dim1];
	ch[*ido + ((k << 2) + 3) * ch_dim1] = cc[*ido + (k + cc_dim2) * 
		cc_dim1] - tr1;
	ch[((k << 2) + 2) * ch_dim1 + 1] = ti1 - cc[*ido + (k + cc_dim2 * 3) *
		 cc_dim1];
	ch[((k << 2) + 4) * ch_dim1 + 1] = ti1 + cc[*ido + (k + cc_dim2 * 3) *
		 cc_dim1];
/* L106: */
    }
L107:
    return 0;
} /* radf4_ */


