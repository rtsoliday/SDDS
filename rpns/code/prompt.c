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
 Revision 1.4  1999/09/14 18:22:06  soliday
 Added rpn.h include statement

 Revision 1.3  1998/08/11 18:27:55  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:22  saunders
 * First test release of the SDDS1.5 package.
 *
*/

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



