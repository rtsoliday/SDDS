/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/08/11 18:27:57  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:31  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* file: rpn_dummy.c
 * contents: rpn_clear(), rpn(), rpn_store()
 * purpose: dummy routines for linking with read_dump() when rpn is not
 *          needed
 * Michael Borland, 1989
 */
#include "mdb.h"

void rpn_clear(void)
{
    bomb("rpn routine clear() called--routine not present", NULL);
    }

double rpn(char *s)
{
    bomb("rpn routine rpn() called--routine not present", NULL);
    return(0.0);
    }


void rpn_store(double value, long mem)
{
    bomb("rpn routine rpn_store() called--routine not present", NULL);
    }


