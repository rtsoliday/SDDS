/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:24  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* routine: m_scmul()
 * purpose: multiply a matrix by a scalar
 * usage  : scmul(B, A, a)  ==>  B=aA
 */
#include "matlib.h"

int mat_scmul(
    MATRIX *B, MATRIX *A,
    double a
    )
{
    register long i, j;
    long a_n, a_m;
    register double afast, *a_i, *b_i;

    afast = a;
    if ((a_n=A->n)!=B->n || (a_m=A->m)!=B->m)
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        b_i = B->a[i];
        for (j=0; j<a_m; j++) 
            b_i[j] = afast*a_i[j];
        }
    return(1);
    }


