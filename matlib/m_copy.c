/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_copy()
 * purpose: copy a matrix
 * usage:
 *   MATRIX *A, *B;
 *   ...
 *   m_copy(A, B);	! copy B to A 
 * Michael Borland, 1986.
 */
#include "matlib.h"

int mat_copy(MATRIX *A, MATRIX *B)
{
    register int i, j, a_m, a_n;
    register double *a_i, *b_i;
  
    if ((a_n=A->n)!=B->n || (a_m=A->m)!=B->m) 
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = (A->a)[i];
        b_i = (B->a)[i];
        for (j=0; j<a_m; j++)
            a_i[j] = b_i[j];
        }
    return(1);
    }


