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
 Revision 1.3  1998/08/11 18:27:53  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:13  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in logical.prot */
/* file    : logical.c
 * contents: greater(), less(), greater_equal(), less_equal(),
 *           equal(), not_equal(), log_and(), log_or(), log_not(),
 *           pop_log, ntol(), lton()
 * purpose : user-callable logical routines for RPN
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"

void greater(void)
{
    if (!stack_test(stackptr, 2, "numeric", "greater"))
        return;
    if (stack[stackptr-2]>stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void less(void)
{
    if (!stack_test(stackptr, 2, "numeric", "less"))
        return;
    if (stack[stackptr-2]<stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void greater_equal(void)
{
    if (!stack_test(stackptr, 2, "numeric", "greater_equal"))
        return;
    if (stack[stackptr-2]>=stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void less_equal(void)
{
    if (!stack_test(stackptr, 2, "numeric", "less_equal"))
        return;
    if (stack[stackptr-2]<=stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void equal(void)
{
    if (!stack_test(stackptr, 2, "numeric", "equal"))
        return;
    if (stack[stackptr-2]==stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void not_equal(void)
{
    if (!stack_test(stackptr, 2, "numeric", "not_equal"))
        return;
    if (stack[stackptr-2]!=stack[stackptr-1])
        logicstack[lstackptr++] = 1;
    else
        logicstack[lstackptr++] = 0;
    }

void log_and(void)
{
    if (!stack_test(lstackptr, 2, "logical", "log_and"))
        return;
    logicstack[lstackptr-2] = (logicstack[lstackptr-1] && logicstack[lstackptr-2]);
    lstackptr--;
    }

void log_or(void)
{
    if (!stack_test(lstackptr, 2, "logical", "log_or"))
        return;
    logicstack[lstackptr-2] = (logicstack[lstackptr-1] || logicstack[lstackptr-2]);
    lstackptr--;
    }

void log_not(void)
{
    if (!stack_test(lstackptr, 1, "logical", "log_not"))
        return;
    logicstack[lstackptr-1] = !logicstack[lstackptr-1];
    }


void poplog(void)
{
    if (!stack_test(lstackptr, 1, "logical", "poplog"))
        return;
    lstackptr--;
    }

void lton(void)
{
    if (!stack_test(lstackptr, 1, "logical", "lton"))
        return;
    stack[stackptr++] = logicstack[lstackptr-1];
    }

void ntol(void)
{
    if (!stack_test(stackptr, 1, "numeric", "ntol"))
        return;
    logicstack[lstackptr++] = (stack[stackptr-1]?1:0);
    }


