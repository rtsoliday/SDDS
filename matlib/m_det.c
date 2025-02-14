/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_det()
 * purpose: compute determinant of a matrix 
 * usage:
 *   MATRIX *A;
 *   double det;
 *   ...
 *   det = m_det(A);
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.5  2002/03/21 16:50:32  shang
 replaced A->a[j][j]!=0 (which is a bug) by A->a[j][i]!=0

 Revision 1.4  2000/01/19 14:43:05  borland
 Fixed memory leak.

 Revision 1.3  1998/04/21 21:26:38  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:12  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

#define TMP a_j

double mat_det(MATRIX *D)
{
    register long i, j, k, n;
    register double det, *a_i, *a_j, a_i_i;
    MATRIX *A;

    det = 1.;
    if ((n=D->n)!=D->m)
        return(0.);

    m_alloc(&A, n, n);
    if (!m_copy(A, D)) {
      m_free(&A);
      return(0.);
    }
    
    /* first diagonalize the matrix */
    for (i=0; i<(n-1); i++) {
        if (A->a[i][i]==0.) {
            for (j=0; j<n; j++) 
                if (A->a[j][i]!=0)  
                    break;
            if (j==n)
                return(0.);
            TMP     = A->a[i];
            A->a[i] = A->a[j];
            A->a[j] = TMP;
            det *= -1.0;
            }
        det *= A->a[i][i];
        a_i = A->a[i];
        a_i_i = a_i[i];
        for (j=i+1; j<n; j++) {
            a_j = A->a[j];
            for (k=i+1; k<n; k++)
                a_j[k] = a_j[k] - a_i[k]*a_j[i]/a_i_i;
            }
        }
    det = det*A->a[i][i];
    m_free(&A);
    return(det);
    }
 

