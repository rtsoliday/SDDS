/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


#include "mdb.h"
#include <ctype.h>

/* routine: escape_quotes()
 * purpose: change " into \" throughout a string
 *
 * Michael Borland, 1994.
 */

char *escape_quotes(char *s)
{
    char *ptr, *bptr;
    static char *buffer=NULL;
    
    if (!s)
        return(s);
    ptr = s;
    buffer = trealloc(buffer, sizeof(*buffer)*(4*(strlen(s)+1)));
    bptr = buffer;

    while (*ptr) {
        if (*ptr=='"' && (ptr==s || *(ptr-1)!='\\'))
            *bptr++ = '\\';
        *bptr++ = *ptr++;
        }
    *bptr = 0;
    strcpy_ss(s, buffer);
    return(s);
    }

        

