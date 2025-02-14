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
 Revision 1.5  2003/06/19 18:20:08  shang
 added push_long and pop_long function

 Revision 1.4  2002/08/14 16:28:58  soliday
 Added Open License

 Revision 1.3  1998/08/11 18:27:55  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 Revision 1.2  1995/09/05 21:22:20  saunders
 First test release of the SDDS1.5 package.

*/

/* prototypes for this file are in pop_push.prot */
/* file    : pop_push.c
 * contents: pop_num(), push_num(), pop_log(), push_log()
 *           pop_file(), push_file()
 *           pop_string(), push_string()
 *           push_code(), pop_code()
 *
 * purpose : stack maintainence subroutines for RPN
 * Michael Borland, 1988
 */
#include "rpn_internal.h"

double pop_num(void)
{
    if (stackptr<1) {
        fputs("too few items on numeric stack (pop_num)\n", stderr);
        rpn_set_error();
        stop();
        return(0.0);
        }
    return(stack[--stackptr]);
    }

long push_long(long num)
{
  if (dstackptr>=STACKSIZE) {
    fputs("stack overflow--numeric stack size exceeded (push_num)\n", stderr);
    rpn_set_error();
    stop();
    return(0);
  }
  dstack[dstackptr++] = num;
  return(1);
}

long pop_long(void)
{
  if (dstackptr<1) {
    fputs("too few items on numeric stack (pop_long)\n", stderr);
    rpn_set_error();
    stop();
    return(0.0);
  }
  return(dstack[--dstackptr]);
}

long push_num(double num)
{
    if (stackptr>=STACKSIZE) {
        fputs("stack overflow--numeric stack size exceeded (push_num)\n", stderr);
        rpn_set_error();
        stop();
        return(0);
        }
    stack[stackptr++] = num;
    return(1);
    }


long pop_log(int32_t *logical)
{
    if (lstackptr<1) {
        fputs("too few items on logical stack (pop_log)\n", stderr);
        rpn_set_error();
        stop();
        return(0);
        }
    *logical = logicstack[--lstackptr];
    return(1);
    }

long push_log(long logical)
{
    if (lstackptr==LOGICSTACKSIZE) {
        fputs("stack overflow--logical stack size exceeded (push_log)\n", stderr);
        rpn_set_error();
        stop();
        return(0);
        }
    logicstack[lstackptr++] = logical;
    return(1);
    }

long pop_file(void)
{
    if (istackptr<1) {
        fputs("too few items on input file stack (pop_file)\n", stderr);
        rpn_set_error();
        stop();
        return(0);
        }
    --istackptr;
    return(1);
    }

long push_file(char *filename)
{
    if (istackptr==FILESTACKSIZE) {
        fputs("stack overflow--input file stack size exceeded (push_file)\n", stderr);
        rpn_set_error();
        stop();
        return(0);
        }
    if ((input_stack[istackptr++].fp = fopen(filename, "r"))==NULL) {
        fprintf(stderr, "unable to open input file %s\n", filename);
        rpn_set_error();
        stop();
        istackptr--;
        return(0);
        }
    return(1);
    }


char *pop_string(void)
{
    if (sstackptr<1) {
        fputs("too few values on string stack (pop_string)\n", stderr);
        rpn_set_error();
        stop();
        return(NULL);
        }
    return(sstack[--sstackptr]);
    }

void push_string(char *s)
{
    register long len;

    if (sstackptr>=STACKSIZE) {
        fputs("string stack overflow (push_string)\n", stderr);
        rpn_set_error();
        stop();
        return;
        }
    if (*s=='"')
       s++;
    if (*(s+(len=strlen(s)-1))=='"')
        *(s+len) = 0;
    cp_str(&(sstack[sstackptr++]), s);
    }

void pop_code(void)
{
#ifdef DEBUG
    fprintf(stderr, "popping code: text=<%s> mode=%ld\n", code_ptr->text, code_ptr->storage_mode);
    fflush(stdout);
#endif
    if (code_lev==1) {
        *(code_ptr->text) = 0;
        code_ptr->position = 0;
        code_ptr->token = NULL;
        return;
        }
    code_lev--;
    if (code_ptr->pred!=NULL) {
        if (code_ptr->storage_mode==VOLATILE) {
#ifdef DEBUG
            fprintf(stderr, "memory free()'d\n");
	    fflush(stdout);
#endif
            tfree(code_ptr->text);
            code_ptr->text = NULL;
            }
        code_ptr = code_ptr->pred;
        }
    }

void push_code(char *code, long mode)
{
    /* If there is still data in the current node's text string, advance
     * to the next node, otherwise just copy the new code onto the current
     * node.
     */

    if (!is_blank(code_ptr->text)) {
        /* if there is no node above the current one, allocate a new node and
         * establish pointer links */
        if (code_ptr->succ==NULL) {
            code_ptr->succ = tmalloc(sizeof(*code_ptr));
            code_ptr->succ->pred = code_ptr;
            code_ptr->succ->buffer = NULL;
            code_ptr->succ->succ = NULL;
            }
        /* advance global node pointer */
        code_ptr = code_ptr->succ;
        code_lev++;
        }

    code_ptr->text = code;
    code_ptr->position = 0;
    code_ptr->token = 0;
    code_ptr->storage_mode = mode;
    if (code_ptr->buffer==NULL)
        code_ptr->buffer =
            tmalloc(sizeof(*(code_ptr->buffer))*LBUFFER);

#ifdef DEBUG
    fprintf(stderr, "pushed code: %s\nmode: %ld\tlevel: %ld\n",
            code, mode, code_lev);
    fflush(stdout);
#endif
    }


