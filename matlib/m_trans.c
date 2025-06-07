/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_trans()
 * purpose: transpose a matrix 
 * usage  : m_trans(B, A) ==> B=TRANS(A)
 * Michael Borland, 1986.
 */
#include "matlib.h"

int mat_trans(MATRIX *B, MATRIX *A)
{
    register long i, j;
    register double *a_i, a_m, a_n;
    
    if (A->m!=B->n || A->n!=B->m)
        return(0);
    a_m = A->m;
    a_n = A->n;
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        for (j=0; j<a_m; j++)
            B->a[j][i] = a_i[j];
        }
    return(1);
    }


