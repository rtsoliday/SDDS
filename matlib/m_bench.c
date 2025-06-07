/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


#include "mdb.h"
#include "matlib.h"

void m_rand(MATRIX *A, double lo, double hi);
void matlib_main()
{
    MATRIX *A, *B, *C;
    MATRIX *At, *Bt;
    long n, ni, i;
    double sum;

    random_1(-1);

    n = query_long("dimension of arrays", 3L); 
    ni = query_long("number of iterations", 100L);

#ifdef VAX_VMS
    init_stats();
#endif

    m_alloc(&A, n, n);
    m_alloc(&At, n, n);
    m_alloc(&B, n, n);
    m_alloc(&Bt, n, n);
    m_alloc(&C, n, n);
    for (i=sum=0; i<ni; i++) {

	m_rand(At, -1.0L, 1.0L);
        m_trans(A, At);

    	m_invert(B, A);
	m_mult(C, A, B);

        sum += fabs(m_det(C)-1.0);

	}
    m_free(&A);
    m_free(&At);
    m_free(&B);
    m_free(&Bt);
    m_free(&C);

    printf("M.A.D.{DET{A.INV(A))}-1} = %e\n", sum/ni);

#ifdef VAX_VMS
    report_stats(stdout, "stats: ");
#endif
    }

void m_rand(
    MATRIX *A,
    double lo, double hi
    )
{
	register long i,j,n;
	register double range;
	double *a_i;
	
	n=A->n;
        range = hi-lo;
	for (i=0; i<n; i++) {
		a_i = A->a[i];
		for (j=0; j<n; j++) 
			a_i[j] = lo + random_1(0)*range;
		}
	}

