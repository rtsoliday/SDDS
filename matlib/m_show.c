/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_show()
 * purpose: to display a matrix on the screen 
 * usage:
 *   MATRIX *A;
 *   ...
 *   m_show(A, "%.4f  ", "label", stdout);   ! print w/a certain format 
 *   
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:26  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "matlib.h"

void mat_show(
    MATRIX *A,
    char *format,
    char *label,
    FILE *fp
    )
{
    register long i, j;

    if (label)
        fputs(label, fp);
    for (i=0; i<A->n; i++) {
        for (j=0; j<A->m; j++)
            fprintf(fp, format, A->a[i][j]);
        fputc('\n', fp);
        }
    }


