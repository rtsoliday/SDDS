/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_free()
 * purpose: free storage allocated to a matrix 
 * usage  :
 *   MATRIX *A;
 *   long n, m;
 *   m_alloc(&A, n, m);
 *     ...
 *   m_free(&A);
 *
 * Michael Borland, 1986.
 */
#include "matlib.h"
#if defined(vxWorks) || defined(__APPLE__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

void mat_free(MATRIX **A)
{
    register long i, n;

    if (!A || !*A || !(*A)->a)
        return;
    n = (*A)->n;
    for (i=0; i<n; i++) {
        if ((*A)->a[i])
            free((*A)->a[i]);
        (*A)->a[i] = NULL;
        }
    free((*A)->a);
    (*A)->a = NULL;
    free(*A);
    *A = NULL;
    }


