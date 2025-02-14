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

 * Revision 1.2  1995/09/05  21:22:33  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* file: rpn_error.c
 * purpose: record occurence of errors in rpn routines
 *
 * Michael Borland, 1994.
 */
#include "rpn_internal.h"

static long error_occurred = 0;

void rpn_set_error()
{
    error_occurred = 1;
    }

long rpn_check_error()
{
    return(error_occurred);
    }

void rpn_clear_error()
{
    error_occurred = 0;
    }

