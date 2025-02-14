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
 Revision 1.2  1995/09/05 21:21:31  saunders
 First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

/* routine: free_namelist_text()
 * purpose: free memory stored in a namelist 
 */


void free_namelist_text(NAMELIST_TEXT *nl)
{
    register long i, j;

    if (nl->group_name) {
#if defined(DEBUG)
        fprintf(stderr, "freeing namelist %s\n", nl->group_name);
#endif
        free(nl->group_name);
        nl->group_name = 0;
        }
    
#if defined(DEBUG)
    fprintf(stderr, "%ld entities\n", nl->n_entities);
#endif
    for (i=0; i<nl->n_entities; i++) {
        if (nl->entity && nl->entity[i]) {
#if defined(DEBUG)
            fprintf(stderr, "freeing entity[%ld]\n", i);
#endif
            free(nl->entity[i]);
            nl->entity[i] = NULL;
            }
        if (nl->repeat && nl->repeat[i]) {
#if defined(DEBUG)
            fprintf(stderr, "freeing repeat[%ld] \n", i);
#endif
            free(nl->repeat[i]);
            nl->repeat[i] = NULL;
            }
        if (nl->value && nl->value[i] && nl->n_values) {
#if defined(DEBUG)
            fprintf(stderr, "freeing %ld value[%ld] array entries\n", nl->n_values[i], i);
#endif
            for (j=0; j<nl->n_values[i]; j++) {
                if (nl->value[i][j]) {
#if defined(DEBUG)
                    fprintf(stderr, "freeing value[%ld][%ld]\n", i, j);
#endif
                    free(nl->value[i][j]);
                    nl->value[i][j] = NULL;
                    }
                }
#if defined(DEBUG)
            fprintf(stderr, "freeing value[%ld]\n", i);
#endif
            free(nl->value[i]);
            nl->value[i] = NULL;
            }
        } 
    
#if defined(DEBUG)
    fprintf(stderr, "freeing toplevel arrays\n\n");
#endif
    if (nl->n_values) free(nl->n_values);
    nl->n_values = NULL;
    if (nl->repeat) free(nl->repeat);
    nl->repeat = NULL;
    if (nl->entity) free(nl->entity);
    nl->entity = NULL;
    if (nl->value) free(nl->value);
    nl->value = NULL;
    if (nl->n_subscripts) free(nl->n_subscripts);
    nl->n_subscripts = NULL;
    if (nl->subscript) free(nl->subscript);
    nl->subscript = NULL;
        
    nl->n_entities = 0;
    }
    

