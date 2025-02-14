/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radf3.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:55  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int radf3_(integer *ido, integer *l1, doublereal *cc, 
	doublereal *ch, doublereal *wa1, doublereal *wa2)
{
    /* Initialized data */

    doublereal taur = -.5;
    doublereal taui = .866025403784439;

    /* System generated locals */
    integer ch_dim1, ch_offset, cc_dim1, cc_dim2, cc_offset, i_1, i_2;

    /* Local variables */
    integer i, k, ic;
    doublereal ci2, di2, di3, cr2, dr2, dr3, ti2, ti3, tr2, tr3;
    integer idp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_dim2 = *l1;
    cc_offset = cc_dim1 * (cc_dim2 + 1) + 1;
    cc -= cc_offset;
    ch_dim1 = *ido;
    ch_offset = (ch_dim1 << 2) + 1;
    ch -= ch_offset;
    --wa1;
    --wa2;

    /* Function Body */
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	cr2 = cc[(k + (cc_dim2 << 1)) * cc_dim1 + 1] + cc[(k + cc_dim2 * 3) * 
		cc_dim1 + 1];
	ch[(k * 3 + 1) * ch_dim1 + 1] = cc[(k + cc_dim2) * cc_dim1 + 1] + cr2;

	ch[(k * 3 + 3) * ch_dim1 + 1] = taui * (cc[(k + cc_dim2 * 3) * 
		cc_dim1 + 1] - cc[(k + (cc_dim2 << 1)) * cc_dim1 + 1]);
	ch[*ido + (k * 3 + 2) * ch_dim1] = cc[(k + cc_dim2) * cc_dim1 + 1] + 
		taur * cr2;
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
	    dr2 = wa1[i - 2] * cc[i - 1 + (k + (cc_dim2 << 1)) * cc_dim1] + 
		    wa1[i - 1] * cc[i + (k + (cc_dim2 << 1)) * cc_dim1];
	    di2 = wa1[i - 2] * cc[i + (k + (cc_dim2 << 1)) * cc_dim1] - wa1[i 
		    - 1] * cc[i - 1 + (k + (cc_dim2 << 1)) * cc_dim1];
	    dr3 = wa2[i - 2] * cc[i - 1 + (k + cc_dim2 * 3) * cc_dim1] + wa2[
		    i - 1] * cc[i + (k + cc_dim2 * 3) * cc_dim1];
	    di3 = wa2[i - 2] * cc[i + (k + cc_dim2 * 3) * cc_dim1] - wa2[i - 
		    1] * cc[i - 1 + (k + cc_dim2 * 3) * cc_dim1];
	    cr2 = dr2 + dr3;
	    ci2 = di2 + di3;
	    ch[i - 1 + (k * 3 + 1) * ch_dim1] = cc[i - 1 + (k + cc_dim2) * 
		    cc_dim1] + cr2;
	    ch[i + (k * 3 + 1) * ch_dim1] = cc[i + (k + cc_dim2) * cc_dim1] + 
		    ci2;
	    tr2 = cc[i - 1 + (k + cc_dim2) * cc_dim1] + taur * cr2;
	    ti2 = cc[i + (k + cc_dim2) * cc_dim1] + taur * ci2;
	    tr3 = taui * (di2 - di3);
	    ti3 = taui * (dr3 - dr2);
	    ch[i - 1 + (k * 3 + 3) * ch_dim1] = tr2 + tr3;
	    ch[ic - 1 + (k * 3 + 2) * ch_dim1] = tr2 - tr3;
	    ch[i + (k * 3 + 3) * ch_dim1] = ti2 + ti3;
	    ch[ic + (k * 3 + 2) * ch_dim1] = ti3 - ti2;
/* L102: */
	}
/* L103: */
    }
    return 0;
} /* radf3_ */


