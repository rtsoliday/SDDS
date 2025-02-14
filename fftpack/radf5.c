/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radf5.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:13:59  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

/* Subroutine */ int radf5_(integer *ido, integer *l1, doublereal *cc, 
	doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3, 
	doublereal *wa4)
{
    /* Initialized data */

    doublereal tr11 = .309016994374947;
    doublereal ti11 = .951056516295154;
    doublereal tr12 = -.809016994374947;
    doublereal ti12 = .587785252292473;

    /* System generated locals */
    integer cc_dim1, cc_dim2, cc_offset, ch_dim1, ch_offset, i_1, i_2;

    /* Local variables */
    integer i, k, ic;
    doublereal ci2, di2, ci4, ci5, di3, di4, di5, ci3, cr2, cr3, dr2, 
	    dr3, dr4, dr5, cr5, cr4, ti2, ti3, ti5, ti4, tr2, tr3, tr4, tr5;
    integer idp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_dim2 = *l1;
    cc_offset = cc_dim1 * (cc_dim2 + 1) + 1;
    cc -= cc_offset;
    ch_dim1 = *ido;
    ch_offset = ch_dim1 * 6 + 1;
    ch -= ch_offset;
    --wa1;
    --wa2;
    --wa3;
    --wa4;

    /* Function Body */
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	cr2 = cc[(k + cc_dim2 * 5) * cc_dim1 + 1] + cc[(k + (cc_dim2 << 1)) * 
		cc_dim1 + 1];
	ci5 = cc[(k + cc_dim2 * 5) * cc_dim1 + 1] - cc[(k + (cc_dim2 << 1)) * 
		cc_dim1 + 1];
	cr3 = cc[(k + (cc_dim2 << 2)) * cc_dim1 + 1] + cc[(k + cc_dim2 * 3) * 
		cc_dim1 + 1];
	ci4 = cc[(k + (cc_dim2 << 2)) * cc_dim1 + 1] - cc[(k + cc_dim2 * 3) * 
		cc_dim1 + 1];
	ch[(k * 5 + 1) * ch_dim1 + 1] = cc[(k + cc_dim2) * cc_dim1 + 1] + cr2 
		+ cr3;
	ch[*ido + (k * 5 + 2) * ch_dim1] = cc[(k + cc_dim2) * cc_dim1 + 1] + 
		tr11 * cr2 + tr12 * cr3;
	ch[(k * 5 + 3) * ch_dim1 + 1] = ti11 * ci5 + ti12 * ci4;
	ch[*ido + (k * 5 + 4) * ch_dim1] = cc[(k + cc_dim2) * cc_dim1 + 1] + 
		tr12 * cr2 + tr11 * cr3;
	ch[(k * 5 + 5) * ch_dim1 + 1] = ti12 * ci5 - ti11 * ci4;
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
	    dr4 = wa3[i - 2] * cc[i - 1 + (k + (cc_dim2 << 2)) * cc_dim1] + 
		    wa3[i - 1] * cc[i + (k + (cc_dim2 << 2)) * cc_dim1];
	    di4 = wa3[i - 2] * cc[i + (k + (cc_dim2 << 2)) * cc_dim1] - wa3[i 
		    - 1] * cc[i - 1 + (k + (cc_dim2 << 2)) * cc_dim1];
	    dr5 = wa4[i - 2] * cc[i - 1 + (k + cc_dim2 * 5) * cc_dim1] + wa4[
		    i - 1] * cc[i + (k + cc_dim2 * 5) * cc_dim1];
	    di5 = wa4[i - 2] * cc[i + (k + cc_dim2 * 5) * cc_dim1] - wa4[i - 
		    1] * cc[i - 1 + (k + cc_dim2 * 5) * cc_dim1];
	    cr2 = dr2 + dr5;
	    ci5 = dr5 - dr2;
	    cr5 = di2 - di5;
	    ci2 = di2 + di5;
	    cr3 = dr3 + dr4;
	    ci4 = dr4 - dr3;
	    cr4 = di3 - di4;
	    ci3 = di3 + di4;
	    ch[i - 1 + (k * 5 + 1) * ch_dim1] = cc[i - 1 + (k + cc_dim2) * 
		    cc_dim1] + cr2 + cr3;
	    ch[i + (k * 5 + 1) * ch_dim1] = cc[i + (k + cc_dim2) * cc_dim1] + 
		    ci2 + ci3;
	    tr2 = cc[i - 1 + (k + cc_dim2) * cc_dim1] + tr11 * cr2 + tr12 * 
		    cr3;
	    ti2 = cc[i + (k + cc_dim2) * cc_dim1] + tr11 * ci2 + tr12 * ci3;
	    tr3 = cc[i - 1 + (k + cc_dim2) * cc_dim1] + tr12 * cr2 + tr11 * 
		    cr3;
	    ti3 = cc[i + (k + cc_dim2) * cc_dim1] + tr12 * ci2 + tr11 * ci3;
	    tr5 = ti11 * cr5 + ti12 * cr4;
	    ti5 = ti11 * ci5 + ti12 * ci4;
	    tr4 = ti12 * cr5 - ti11 * cr4;
	    ti4 = ti12 * ci5 - ti11 * ci4;
	    ch[i - 1 + (k * 5 + 3) * ch_dim1] = tr2 + tr5;
	    ch[ic - 1 + (k * 5 + 2) * ch_dim1] = tr2 - tr5;
	    ch[i + (k * 5 + 3) * ch_dim1] = ti2 + ti5;
	    ch[ic + (k * 5 + 2) * ch_dim1] = ti5 - ti2;
	    ch[i - 1 + (k * 5 + 5) * ch_dim1] = tr3 + tr4;
	    ch[ic - 1 + (k * 5 + 4) * ch_dim1] = tr3 - tr4;
	    ch[i + (k * 5 + 5) * ch_dim1] = ti3 + ti4;
	    ch[ic + (k * 5 + 4) * ch_dim1] = ti4 - ti3;
/* L102: */
	}
/* L103: */
    }
    return 0;
} /* radf5_ */


