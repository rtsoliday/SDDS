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


