/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* prototypes for this file are in get_token_rpn.prot */
/* file: get_token_rpn.c
 * contents: get_token()
 * purpose : special tokenizer for use with rpn.
 *           -> puts tokens in a user-supplied buffer
 *           -> doesn't copy string over itself (non-destructive)
 *
 * Michael Borland, 1988
 */
#include "mdb.h"
#include "rpn.h"
#include <ctype.h>

#define skip_it(m_c) (isspace(m_c) || (m_c)==',' || (m_c)==';')
#define nskip_it(m_c) (!isspace(m_c) && (m_c)!=',' && (m_c)!=';')

char *get_token_rpn(
    char *s,		   /* string being scanned */
    char *buf,             /* buffer for token     */
    long lbuf,              /* buffer length */
    long *spos              /* position of scanning (updated on exit) */
    )
{
    char *ptr0, *ptr1;

    /* skip leading white-space and commas */
    ptr0 = s = s + *spos;
    while (*s && skip_it(*s))
        s++;
    if (*s==0)
        return(NULL);
    ptr1 = s;

    if (*s=='"') {
        ptr1 = s;
        /* if quoted string, skip to next quotation mark */
        do {
            s++;
            } while (*s!='"' && *s);
        if (*s=='"')
            s++;
        }
    else {
        /* skip to first white-space following token */
        do {
            s++;
            } while (*s && nskip_it(*s));
        }

    if ((s-ptr1+1)>lbuf) {
        fprintf(stderr, "buffer overflow in get_token_rpn()\nstring was %s\n", ptr0);
        exit(1);
        }
    strncpy(buf, ptr1, s-ptr1);
    buf[s-ptr1]=0;

    *spos += s-ptr0;
    return(buf);
    }

