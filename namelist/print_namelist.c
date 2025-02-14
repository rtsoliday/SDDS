/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

void print_namelist_output(char *buffer, long *column, FILE *fp);
void print_namelist_tags(long *end_required, long *first_item, long *first_value, long *column,
                         char *nlname, ITEM *item, FILE *fp);
long containsWhitespace(char *string);

static long pn_flags = 0;

void set_print_namelist_flags(long flags)
{
    pn_flags = flags;
    }

/* routine: print_namelist()
 * purpose: print a namelist 
 */


void print_namelist(FILE *fp, NAMELIST *nl)
{
    long i, j;
    long n_values;
    register ITEM *item;
    register char *ptr, *dptr;
    short a_short;
    long a_long;
    int a_int;
    int32_t a_int32;
    float a_float;
    double a_double;
    char a_char;
    long column, first_item, first_value, end_required;
    char buffer[16384], buffer2[16384];

#if defined(DEBUG)
    printf("working on namelist %s\n", nl->name);
    fflush(stdout);
#endif
    first_item = 1;
    end_required = 0;
    column = 0;
    for (i=0; i<nl->n_items; i++) {
        first_value = 1;
        item = nl->item_list+i;
        n_values = 1;
        for (j=0; j<item->n_subscripts; j++)
            n_values *= item->dimensions[j];
        if (!(ptr  = item->root))
            bomb("pointer to root of data item not found", NULL);
        if (!(dptr = item->def_root))
            bomb("pointer to root of default data not found", NULL);
#if defined(DEBUG)
        printf("working on item %ld, name %s\n", i, item->name);
        fflush(stdout);
#endif
        switch (item->type) {
          case TYPE_SHORT:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((short*)ptr)!=*((short*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%hd,%c", *((short*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_short);
                dptr += sizeof(a_short);
                }
            break;
          case TYPE_INT:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((int*)ptr)!=*((int*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%d,%c", *((int*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_int);
                dptr += sizeof(a_int);
                }
            break;
          case TYPE_INT32_T:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((int32_t*)ptr)!=*((int32_t*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%" PRId32 ",%c", *((int32_t*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_int32);
                dptr += sizeof(a_int32);
                }
            break;
          case TYPE_LONG:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((long*)ptr)!=*((long*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%ld,%c", *((long*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_long);
                dptr += sizeof(a_long);
                }
            break;
          case TYPE_FLOAT:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((float*)ptr)!=*((float*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%.8e,%c", *((float*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_float);
                dptr += sizeof(a_float);
                }
            break;
          case TYPE_DOUBLE:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((double*)ptr)!=*((double*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "%.15e,%c", *((double*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_double);
                dptr += sizeof(a_double);
                }
            break;
          case TYPE_STRING:
            for (j=0; j<n_values; j++) {
#if defined(DEBUG)
                printf("type_string: *ptr = %x  *dptr = %x\n",
                       *((STRING*)ptr), *((STRING*)dptr));
                if (*((STRING*)ptr)==NULL)
                    puts("*ptr = NULL");
                else 
                    printf("*ptr = >%s<\n", *((STRING*)ptr));
                if (*((STRING*)dptr)==NULL)
                    puts("*dptr = NULL");
                else 
                    printf("*dptr = >%s<\n", *((STRING*)dptr));
#endif
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 ||
                    (!(*((STRING*)ptr)==NULL && *((STRING*)dptr)==NULL) &&
                        ( (*((STRING*)dptr)==NULL && *((STRING*)ptr)!=NULL) ||
                         (*((STRING*)dptr)!=NULL && *((STRING*)ptr)==NULL) ||
                         strcmp(*((STRING*)ptr), *((STRING*)dptr))!=0))) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    strcpy_ss(buffer2, *((STRING*)ptr)?*((STRING*)ptr):"{NULL}");
                    escape_quotes(buffer2);
                    if (containsWhitespace(buffer2) || strlen(buffer2)==0 || strpbrk(buffer2, "$\",&"))
                      snprintf(buffer, sizeof(buffer), "\"%.*s\",%c", 
                               (int)(sizeof(buffer) - 5), buffer2, j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' ');
                    else
                      snprintf(buffer, sizeof(buffer), "%.*s,%c", 
                               (int)(sizeof(buffer) - 3), buffer2, j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' ');
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(ptr);
                dptr += sizeof(ptr);
                }
            break;
          case TYPE_CHAR:
            for (j=0; j<n_values; j++) {
                if (!(pn_flags&PRINT_NAMELIST_NODEFAULTS) || n_values!=1 || *((char*)ptr)!=*((char*)dptr)) {
                    print_namelist_tags(&end_required, &first_item, &first_value, &column, nl->name, item, fp);
                    sprintf(buffer, "\"%c\",%c", *((char*)ptr), (j==n_values-1?(pn_flags&PRINT_NAMELIST_COMPACT?' ':'\n'):' '));
                    print_namelist_output(buffer, &column, fp);
                    }
                ptr  += sizeof(a_char);
                dptr += sizeof(a_char);
                }
            break;
          default:
            /* something extra that we don't handle here */
            break;
            }
        }
    if (first_item)
        fprintf(fp, "&%s &end\n", nl->name);
    else if (end_required) {
        fprintf(fp, "&end\n");
        }
    fflush(fp);
    }

void print_namelist_output(char *buffer, long *column, FILE *fp)
{
    long length;
    
    if (((length = strlen(buffer))+ *column)>120) {
        if (pn_flags&PRINT_NAMELIST_COMPACT) {
            fputs("\n ", fp);
            *column = 2;
            }
        else {
            fputs("\n        ", fp);
            *column = 9;
            }
        }
    fputs(buffer, fp);
    *column += strlen(buffer);
    }

void print_namelist_tags(long *end_required, long *first_item, long *first_value, long *column,
                         char *nlname, ITEM *item, FILE *fp)
{
    static char buffer[16384];
    long i;

    if (*first_item) {
        fprintf(fp, "&%s\n", nlname);
        *end_required = 1;
        *first_item = 0;
        }
    if (*first_value) {
        if (pn_flags&PRINT_NAMELIST_COMPACT) {
            if ((long)(strlen(item->name)+3+*column)>120) {
                fputs("\n ", fp);
                *column = 2;
                }
            sprintf(buffer, " %s", item->name);
            }
        else {
            sprintf(buffer, "    %s", item->name);
            *column = 0;
            }
        for (i=0; i<item->n_subscripts; i++)
            strcat(buffer, "[0]");
        strcat(buffer, " = ");
        *column += strlen(buffer);
        fputs(buffer, fp);
        *end_required = 1;
        *first_value = 0;
        }
    }

long containsWhitespace(char *string)
{
    if (!string)
        return(0);
    while (*string) {
        if (isspace(*string))
            return(1);
        string++;
        }
    return(0);
    }
