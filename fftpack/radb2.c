/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* radb2.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)

*/

#include "f2c.h"

/* Subroutine */ int radb2_(integer *ido, integer *l1, doublereal *cc, 
	doublereal *ch, doublereal *wa1)
{
    /* System generated locals */
    integer cc_dim1, cc_offset, ch_dim1, ch_dim2, ch_offset, i_1, i_2;

    /* Local variables */
    integer i, k, ic;
    doublereal ti2, tr2;
    integer idp2;

    /* Parameter adjustments */
    cc_dim1 = *ido;
    cc_offset = cc_dim1 * 3 + 1;
    cc -= cc_offset;
    ch_dim1 = *ido;
    ch_dim2 = *l1;
    ch_offset = ch_dim1 * (ch_dim2 + 1) + 1;
    ch -= ch_offset;
    --wa1;

    /* Function Body */
    i_1 = *l1;
    for (k = 1; k <= i_1; ++k) {
	ch[(k + ch_dim2) * ch_dim1 + 1] = cc[((k << 1) + 1) * cc_dim1 + 1] + 
		cc[*ido + ((k << 1) + 2) * cc_dim1];
	ch[(k + (ch_dim2 << 1)) * ch_dim1 + 1] = cc[((k << 1) + 1) * cc_dim1 
		+ 1] - cc[*ido + ((k << 1) + 2) * cc_dim1];
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
	    ch[i - 1 + (k + ch_dim2) * ch_dim1] = cc[i - 1 + ((k << 1) + 1) * 
		    cc_dim1] + cc[ic - 1 + ((k << 1) + 2) * cc_dim1];
	    tr2 = cc[i - 1 + ((k << 1) + 1) * cc_dim1] - cc[ic - 1 + ((k << 1)
		     + 2) * cc_dim1];
	    ch[i + (k + ch_dim2) * ch_dim1] = cc[i + ((k << 1) + 1) * cc_dim1]
		     - cc[ic + ((k << 1) + 2) * cc_dim1];
	    ti2 = cc[i + ((k << 1) + 1) * cc_dim1] + cc[ic + ((k << 1) + 2) * 
		    cc_dim1];
	    ch[i - 1 + (k + (ch_dim2 << 1)) * ch_dim1] = wa1[i - 2] * tr2 - 
		    wa1[i - 1] * ti2;
	    ch[i + (k + (ch_dim2 << 1)) * ch_dim1] = wa1[i - 2] * ti2 + wa1[i 
		    - 1] * tr2;
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
	ch[*ido + (k + ch_dim2) * ch_dim1] = cc[*ido + ((k << 1) + 1) * 
		cc_dim1] + cc[*ido + ((k << 1) + 1) * cc_dim1];
	ch[*ido + (k + (ch_dim2 << 1)) * ch_dim1] = -(cc[((k << 1) + 2) * 
		cc_dim1 + 1] + cc[((k << 1) + 2) * cc_dim1 + 1]);
/* L106: */
    }
L107:
    return 0;
} /* radb2_ */


