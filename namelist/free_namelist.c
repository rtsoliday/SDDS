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
 Revision 1.3  1999/12/15 16:43:34  soliday
 Added vxWorks support.

 Revision 1.2  1995/09/05 21:21:29  saunders
 First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "match_string.h"
#include "namelist.h"
#include <ctype.h>
#if !defined(vxWorks)
#include <memory.h>
#endif

void free_namelist(NAMELIST *nl)
{
    register long i, j, n_values;
    register ITEM *item;
    char **ptr, **dptr;
    long n_items;

    n_items = nl->n_items;
    for (i=0; i<n_items; i++) {
        item = nl->item_list+i;
        n_values = 1;
        if (item->dimensions)
            for (j=0; j<item->n_subscripts; j++)
                n_values *= item->dimensions[j];
        if (item->type==TYPE_STRING && (ptr = (char**)item->root)) {
            dptr = (char**)item->def_root;
            while (n_values--) {
                if (*ptr && (!dptr || *ptr!=*dptr))
                    free(*ptr);
                ptr++;
                dptr++;
                }
            }
        }
    }


