/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_mult()
 * purpose: multiply two matrices
 * usage  : m_mult(C, A, B)  ==>  C= AB
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:23  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

int mat_mult(
    MATRIX *C, MATRIX *A, MATRIX *B
    )
{
    register long i, j, k;
    long n, m, p;
    register double *a_i, *c_i, c_i_j;

    if ((m=A->m)!=B->n || (n=A->n)!=C->n || (p=B->m)!=C->m) 
        return(0);
    for (i=0; i<n; i++) {
        a_i = A->a[i];
        c_i = C->a[i];
        for (j=0; j<p; j++) {
            c_i_j = 0;
            for (k=0; k<m; k++) 
	        c_i_j += a_i[k]*B->a[k][j];
            c_i[j] = c_i_j;
            }
        }
    return(1);
    }


