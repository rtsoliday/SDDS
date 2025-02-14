/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_identity()
 * purpose: set elements of a matrix to zero.
 * usage:
 *  MATRIX *A;
 *  ...
 *  m_identity(A);
 *
 * Michael Borland, 1989.
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:40  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:19  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

void mat_identity(MATRIX *A)
{
    register long i, j, n, m;
    register double *a_i;

    n = A->n;
    m = A->m;
    for (i=0; i<n; i++) {
        a_i = A->a[i];
        for (j=0; j<m; j++) 
            a_i[j] = (i==j?1:0);
        }
    }

