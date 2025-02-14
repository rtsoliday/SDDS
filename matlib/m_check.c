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
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:38  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:09  saunders
 * First test release of the SDDS1.5 package.
 *
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


