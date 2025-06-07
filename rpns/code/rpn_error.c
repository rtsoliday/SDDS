/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* file: rpn_error.c
 * purpose: record occurrence of errors in rpn routines
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

