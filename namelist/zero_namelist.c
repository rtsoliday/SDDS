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
 Revision 1.2  1995/09/05 21:21:48  saunders
 First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

/* routine: zero_namelist()
 * purpose: zero a namelist
 */

void zero_namelist(NAMELIST_TEXT *nl)
{
    nl->n_entities = 0;
    nl->group_name = 0;
    nl->entity     = 0;
    nl->n_values   = 0;
    nl->value      = 0;
    nl->repeat     = 0;
    }



