/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "matlib.h"

/* routine: m_add()
 * purpose: add two matrices
 * usage  : m_add(C, A, B) ==> C = A+B
 * Michael Borland, 1986.
*/

int mat_add(MATRIX *C, MATRIX *A, MATRIX *B)
{
    register int i, j;
    int a_n, a_m;
    register double *a_i, *b_i, *c_i;

    if ((a_n=A->n)!=B->n || B->n!=C->n || (a_m=A->m)!=B->m || B->m!=C->m)
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        b_i = B->a[i];
        c_i = C->a[i];
        for (j=0; j<a_m; j++) 
            c_i[j] = a_i[j] + b_i[j];
        }
     return(1);
     }

/* routine: m_subtract()
 * purpose: subtract two matrices
 * usage  : m_subtract(C, A, B) ==> C = A-B
 * Michael Borland, 1986.
*/

int mat_subtract(MATRIX *C, MATRIX *A, MATRIX *B)
{
    register int i, j;
    int a_n, a_m;
    register double *a_i, *b_i, *c_i;

    if ((a_n=A->n)!=B->n || B->n!=C->n || (a_m=A->m)!=B->m || B->m!=C->m)
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        b_i = B->a[i];
        c_i = C->a[i];
        for (j=0; j<a_m; j++) 
            c_i[j] = a_i[j] - b_i[j];
        }
     return(1);
     }

