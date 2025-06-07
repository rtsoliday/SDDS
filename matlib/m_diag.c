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
void mat_diag(MATRIX *A, double *diagonal_elements) {
    long i;
    
    if(!A||!A->a)
        bomb("Problem with allocation of matrix.\n",NULL);
    if(!diagonal_elements)
        bomb("Problem with allocation of vector of diagonal elements.\n",NULL);
    m_zero(A);
    for(i=0;i<MIN(A->n,A->m);i++)
        A->a[i][i]=diagonal_elements[i];
    }


