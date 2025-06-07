/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_check()
 * purpose: check a matrix for proper structure
 * usage:
 *   MATRIX *A;
 *   ...
 *   m_copy(A);
 * Michael Borland, 1986.
 */
#include "matlib.h"

int mat_check(MATRIX *A)
{
    register int i, a_n;
  
    a_n = A->n;
    if (!(A->a))
        return(0);
    for (i=0; i<a_n; i++) {
        if (!(A->a)[i])
            return(0);
        }
    return(1);
    }


