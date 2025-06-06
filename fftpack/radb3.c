/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radb3.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)

*/

#include "f2c.h"

/* Subroutine */ int radb3_(integer *ido, integer *l1, doublereal *cc, 
	doublereal *ch, doublereal *wa1, doublereal *wa2)
{
    /* Initialized data */

    doublereal taur = -.5;
    doublereal taui = .866025403784439;

    /* System generated locals */
    integer cc_dim1, cc_offset, ch_dim1, ch_dim2, ch_offset, i_1, i_2;

    /* Local variables */
    integer i, k, ic;
    doublereal ci2, ci3, di2, di3, cr2, cr3, dr2, dr3, ti2, tr2;
    integer idp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_offset = (cc_dim1 << 2) + 1;
    cc -= cc_offset;
    ch_dim1 = *ido;
    ch_dim2 = *l1;
    ch_offset = ch_dim1 * (ch_dim2 + 1) + 1;
    ch -= ch_offset;
    --wa1;
    --wa2;

    /* Function Body */
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	tr2 = cc[*ido + (k * 3 + 2) * cc_dim1] + cc[*ido + (k * 3 + 2) * 
		cc_dim1];
	cr2 = cc[(k * 3 + 1) * cc_dim1 + 1] + taur * tr2;
	ch[(k + ch_dim2) * ch_dim1 + 1] = cc[(k * 3 + 1) * cc_dim1 + 1] + tr2;

	ci3 = taui * (cc[(k * 3 + 3) * cc_dim1 + 1] + cc[(k * 3 + 3) * 
		cc_dim1 + 1]);
	ch[(k + (ch_dim2 << 1)) * ch_dim1 + 1] = cr2 - ci3;
	ch[(k + ch_dim2 * 3) * ch_dim1 + 1] = cr2 + ci3;
/* L101: */
    }
    if (*ido == 1) {
	return 0;
    }
    idp2 = *ido + 2;
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	i_2 = *ido;
	for (i = 3; i <= i_2; i += 2) {
	    ic = idp2 - i;
	    tr2 = cc[i - 1 + (k * 3 + 3) * cc_dim1] + cc[ic - 1 + (k * 3 + 2) 
		    * cc_dim1];
	    cr2 = cc[i - 1 + (k * 3 + 1) * cc_dim1] + taur * tr2;
	    ch[i - 1 + (k + ch_dim2) * ch_dim1] = cc[i - 1 + (k * 3 + 1) * 
		    cc_dim1] + tr2;
	    ti2 = cc[i + (k * 3 + 3) * cc_dim1] - cc[ic + (k * 3 + 2) * 
		    cc_dim1];
	    ci2 = cc[i + (k * 3 + 1) * cc_dim1] + taur * ti2;
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + (k * 3 + 1) * cc_dim1] + 
		    ti2;
	    cr3 = taui * (cc[i - 1 + (k * 3 + 3) * cc_dim1] - cc[ic - 1 + (k *
		     3 + 2) * cc_dim1]);
	    ci3 = taui * (cc[i + (k * 3 + 3) * cc_dim1] + cc[ic + (k * 3 + 2) 
		    * cc_dim1]);
	    dr2 = cr2 - ci3;
	    dr3 = cr2 + ci3;
	    di2 = ci2 + cr3;
	    di3 = ci2 - cr3;
	    ch[i - 1 + (k + (ch_dim2 << 1)) * ch_dim1] = wa1[i - 2] * dr2 - 
		    wa1[i - 1] * di2;
	    ch[i + (k + (ch_dim2 << 1)) * ch_dim1] = wa1[i - 2] * di2 + wa1[i 
		    - 1] * dr2;
	    ch[i - 1 + (k + ch_dim2 * 3) * ch_dim1] = wa2[i - 2] * dr3 - wa2[
		    i - 1] * di3;
	    ch[i + (k + ch_dim2 * 3) * ch_dim1] = wa2[i - 2] * di3 + wa2[i - 
		    1] * dr3;
/* L102: */
	}
/* L103: */
    }
    return 0;
} /* radb3_ */


