/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in prompt.prot */
/* routine: prompt()
 * purpose: prints prompt string (first argument) if second argument is != 0
 *
 * Michael Borland, 1988
 */
#include "mdb.h"
#include "rpn.h"

long prompt(char *prompt_s, long do_prompt)
{
    if (do_prompt)
        fputs(prompt_s, stdout);
    return(do_prompt);
    }



