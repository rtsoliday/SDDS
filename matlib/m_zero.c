/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_zero()
 * purpose: set elements of a matrix to zero.
 * usage:
 *  MATRIX *A;
 *  ...
 *  m_zero(A);
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:42  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:30  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

void mat_zero(MATRIX *A)
{
    register long i, j, n, m;
    register double *a_i;

    if (!A || !A->a)
        m_error("null matrix passed to m_zero()");
    n = A->n;
    m = A->m;
    for (i=0; i<n; i++) {
        if (!(a_i = A->a[i]))
            m_error("null pointer in matrix passed to m_zero()");
        for (j=0; j<m; j++) 
            a_i[j] = 0;
        }
    }

