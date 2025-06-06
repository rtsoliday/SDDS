/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_alloc()
 * purpose: allocate storage for a matrix 
 * usage  : 
 *   MATRIX *A;
 *   int n, m;
 *   m_alloc(&A, n, m)
 * 
 * Michael Borland, 1986.
 */
#include "mdb.h"
#include "matlib.h"

void mat_alloc(MATRIX **A, int n, int m)
{
    register long i;
  
    if (n<=0 || m<=0) {
      fprintf(stderr, "error in m_alloc: %d x %d array requested\n", n, m);
      exit(1);
    }
    if ((*A = (MATRIX*)tmalloc(sizeof(**A)))) {
        if (((*A)->a = (double**)tmalloc(sizeof(double*)*n))) {
            (*A)->n = n;
            (*A)->m = m;
            if (m!=0) {
                /* m==0 means only row pointers were wanted */
                for (i=0; i<n; i++) {
                    if (!((*A)->a[i] = (double*)tmalloc(m*sizeof(double))))
	                break;
                    }
                if (i==n) {
                    m_zero(*A);
                    return;
                    }
                }
            }
        }
    puts("Allocation failure in m_alloc().");
    abort();
    }

void m_alloc1(MATRIX **A, int n, int m)
{
    if (n<=0 || m<=0) {
      fprintf(stderr, "error in m_alloc1: %d x %d array requested\n", n, m);
      exit(1);
    }
    if (!*A || !(*A)->a)
        m_alloc(A, n, m);
    else if ((*A)->n!=n || (*A)->m!=m) {
        m_free(A);
        m_alloc(A, n, m);
        }
    }


