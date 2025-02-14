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
 Revision 1.2  1995/09/05 21:21:47  saunders
 First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

/* routine: show_namelist()
 * purpose: display a namelist on a specified output 
 *
 */

void show_namelist(FILE *fp, NAMELIST_TEXT *nl)
{
    register long i, j;

    if (nl->group_name)
        fprintf(fp, "%s\n", nl->group_name);
    fprintf(fp, "%ld entities\n", nl->n_entities);
     
    for (i=0; i<nl->n_entities; i++) {
        fprintf(fp, "entity %4ld: %s\n", i, nl->entity[i]);
        fprintf(fp, "%ld values\n", nl->n_values[i]);
        for (j=0; j<nl->n_values[i]; j++) 
            fprintf(fp, "\t%4ld: %ld*<%s>\n", j, nl->repeat[i][j],
                nl->value[i][j]);
        fprintf(fp, "%ld subscripts\n", nl->n_subscripts[i]);
        for (j=0; j<nl->n_subscripts[i]; j++) 
            fprintf(fp, "\t%4ld: %ld\n", j, nl->subscript[i][j]);
        }
    }


