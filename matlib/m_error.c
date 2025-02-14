/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_error()
 * purpose: report an error condition from a matrix routine
 * usage:
 * if (m_xxxx(...)!=1)
 *   m_error("...");	! exits from program 
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.4  1998/04/21 21:26:39  borland
 New names to allow concurrent use with the Meschach library.

 Revision 1.3  1997/09/08 23:38:36  borland
 Error printouts now go to stderr.

 * Revision 1.2  1995/09/05  21:16:15  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "matlib.h"

void mat_error(char *s)
{
    fprintf(stderr, "matrix error: %s\n", s);
    exit(1);
    }

int p_materror(char *message)
{
    fprintf(stderr, "matrix error: %s\n", message);
    return(0);
    }


