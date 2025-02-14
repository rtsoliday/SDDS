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
 Revision 1.2  2002/08/14 16:23:50  soliday
 Added Open License

 Revision 1.1  1996/02/09 04:31:49  borland
 Added two routines in this module to support -describeInput option on
 sdds*experiment programs; allows listing namelist names, field names,
 types, and default values.

 *
 */
#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

/* must agree with #defines in namelist.h ! */

static char *namelistTypeName[TYPE_CHAR+1] = {
    "", "short", "int", "int32_t", "long", "float", "double", "string", "char"
        } ;

void show_namelist_fields(FILE *fp, NAMELIST *nl, char *nl_name)
{
    long i;
    char **ptr;
    ITEM *item;
    
    fprintf(fp, "*** Namelist %s:\n", nl_name);
    for (i=0; i<nl->n_items; i++) {
        item = nl->item_list+i;
        fprintf(fp, "    %30s: type=%10s, default=",
                item->name, 
                namelistTypeName[item->type]);
        if (item->n_subscripts)
            fprintf(fp, "<array>\n");
        else if (!item->def_root) {
            fprintf(fp, "<none defined>\n");
            }
        else {
            switch (item->type) {
              case TYPE_SHORT:
                fprintf(fp, "%hd\n", *((short*)item->def_root));
                break;
              case TYPE_INT:
                fprintf(fp, "%d\n", *((int*)item->def_root));
                break;
              case TYPE_INT32_T:
                fprintf(fp, "%" PRId32 "\n", *((int32_t*)item->def_root));
                break;
              case TYPE_LONG:
                fprintf(fp, "%ld\n", *((long*)item->def_root));
                break;
              case TYPE_FLOAT:
                fprintf(fp, "%g\n", *((float*)item->def_root));
                break;
              case TYPE_DOUBLE:
                fprintf(fp, "%g\n", *((float*)item->def_root));
                break;
              case TYPE_STRING:
                ptr = (char**)item->def_root;
                if (!*ptr)
                    fprintf(fp, "<null>\n");
                else
                    fprintf(fp, "%s\n", *ptr);
                break;
              case TYPE_CHAR:
                fprintf(fp, "%c\n", *((char*)item->def_root));
                break;
              default:
                break;
                }
            }
        }
    }


void show_namelists_fields(FILE *fp, NAMELIST **nl, char **nl_name, long n_nl)
{
    long i;
    for (i=0; i<n_nl; i++)  {
        show_namelist_fields(fp, nl[i], nl_name[i]);
        fflush(fp);
        }
    }
