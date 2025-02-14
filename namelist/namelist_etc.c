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
 Revision 1.3  2002/08/14 16:23:49  soliday
 Added Open License

 Revision 1.2  1995/09/05 21:21:35  saunders
 First test release of the SDDS1.5 package.

*/

/* file    : namelist_etc.c
 * contents: count_occurences(), un_quote(), extract_subscripts()
 *           is_quoted()
 * purpose : utilities for namelist scanning routines for C
 *
 * Michael Borland, 1988
 */ 
#include "mdb.h"
#include <ctype.h>

/* routine: count_occurences()
 * purpose: returns the number of occurences of a given character in a
 *          string, up to an occurence of one of a specified string of
 *          end characters
 */

long count_occurences(char *s, char c, char *end)
{
    register char *ptr_s, *ptr_e;
    register long count, end_flag;

    ptr_s = s;
    count = 0;
    do {
        while (*ptr_s=='"') {
            do {
                ++ptr_s;
                } while (*ptr_s && !(*ptr_s=='"' && *(ptr_s-1)!='\\'));
            if (*ptr_s=='"')
                ptr_s++;
            }
        if (!*ptr_s)
            break;
        end_flag = 0;
        ptr_e = end;
        while (*ptr_e)
            end_flag = (*ptr_e++==*ptr_s);
        if (end_flag)
            return(count);
        if (*ptr_s==c)
            count++;
        } while (*++ptr_s);
    return(count);
    }


/* routine: un_quote()
 * purpose: remove bounding quotation marks from a string
 */

void un_quote(char *s)
{
    register char *ptr;

    ptr = s;
    if (*ptr!='"')
        return;
    while (*ptr)
        ptr++;
    if (*--ptr=='"')
        *ptr = 0;
    strcpy_ss(s, s+1);
    }

/* routine: extract_subscripts()
 * purpose: extract array of subscript values from an entity name.
 *          Returns the number of subscripts. The subscript values
 *          are removed from the entity name and put into an array.
 */

long extract_subscripts(char *name, long **subscript)
{
    register char *ptr;
    register long n_subscripts;

    n_subscripts = 0;
    ptr = name;
    while (*ptr) {
        if (*ptr=='[' || *ptr=='(' || *ptr==',') 
            n_subscripts++;
        ptr++;
        }
    if (n_subscripts==0) {
        *subscript = 0;
        return(0);
        }

    *subscript = tmalloc(sizeof(**subscript)*n_subscripts);
    ptr = name;
    n_subscripts = 0;
    while (*ptr) {
        if (*ptr=='[' || *ptr=='(' || *ptr==',') {
            *ptr++ = 0;
            sscanf(ptr, "%ld", *subscript+n_subscripts++);
            }
        else
            ptr++;
        }
    return(n_subscripts);
    }

long is_quoted(char *string, char *position, char quotation_mark)
{
    long in_quoted_section;
    char *string0;

    if (*position==quotation_mark)
        return(1);

    in_quoted_section = 0;
    string0 = string;
    while (*string) {
        if (*string==quotation_mark && (string==string0 || *(string-1)!='\\')) 
            in_quoted_section = !in_quoted_section; 
        else if (string==position)
            return(in_quoted_section);
        string++;
        }
    return(0);
    }

char *next_unquoted_char(char *ptr, char c, char quote_mark)
{
    long in_quotes=0;
    char *ptr0;
    ptr0 = ptr;
    do {
        if (*ptr==quote_mark && (ptr==ptr0 || *(ptr-1)!='\\'))
            in_quotes = !in_quotes;
        else if (*ptr==c && !in_quotes)
            return(ptr);
        ptr++;
        } while (*ptr);
    return(NULL);
    }



