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
 Revision 1.4  2002/08/14 16:29:00  soliday
 Added Open License

 Revision 1.3  1998/08/11 18:27:59  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:45  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in stack.prot */
/* file    : stack.c
 * contents: swap(), duplicate(), stack_lev(), pop(), rpn_clear(), rdn(), rup()
 * purpose : miscellaneous user-called stack manipulating routines
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"

void swap(void)
{
    double tmp;
    if (stackptr<2) {
        fputs("too few items on stack (swap)\n", stderr);
        return;
        }
    tmp               = stack[stackptr-1];
    stack[stackptr-1] = stack[stackptr-2];
    stack[stackptr-2] = tmp;
    }

void duplicate(void)
{
    if (stackptr<1) {
        fputs("too few items on stack (duplicate)\n", stderr);
        return;
        }
    push_num(stack[stackptr-1]);
    }

void nduplicate(void)
{
    long i, n;
    if (stackptr<1) {
        fputs("too few items on stack (nduplicate)\n", stderr);
        return;
        }
    n = stack[--stackptr];
    if (stackptr<n) {
        fputs("too few items on stack (nduplicate)\n", stderr);
        return;
        }
    for (i=0; i<n; i++)
        push_num(stack[stackptr-n]);
    }

void stack_lev(void)
{
  stack[stackptr] = stackptr;
  stackptr++;
}

void pop(void)
{
    if (stackptr<1) {
        fputs("too few items on stack (pop)\n", stderr);
        return;
        }
    stackptr--;
    }

void rpn_clear(void)
{
  register long i;
  stackptr = 0;
  for (i=sstackptr-1; i>=0; i--) {
    if (sstack[i])
      free(sstack[i]);
    sstack[i] = NULL;
  }
  sstackptr = 0;
  dstackptr = 0;
  stackptr = 0;
  lstackptr = 0;
}

void pops(void)
{
    if (sstackptr<1) {
        fputs("too few items on string stack (pops)\n", stderr);
        rpn_set_error();
        stop();
        return;
        }
     sstackptr--;
     }

void rup(void)
{
    register long i;

    for (i=stackptr; i>0; i--)
        stack[i] = stack[i-1];
    stack[0] = stack[stackptr];
    }

void rdn(void)
{
    register long i;

    stack[stackptr] = stack[0];
    for (i=0; i<stackptr; i++)
        stack[i] = stack[i+1];
    }

void dup_str(void)
{
    if (sstackptr<1) {
        fputs("too few items on stack (dup_str)\n", stderr);
        return;
        }
    push_string(sstack[sstackptr-1]);
    }

void exe_str(void)
{
    char *code;
    if (!(code = pop_string()))
        return;
    push_code(code, VOLATILE);
    }


