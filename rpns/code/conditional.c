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
 Revision 1.5  1999/06/01 14:37:33  soliday
 Removed warnings when compiled under Linux.

 Revision 1.4  1998/08/11 18:27:52  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 Revision 1.3  1998/06/10 23:58:54  borland
 Fixed a memory leak by declaring branch of conditional to be volatile
 text.

 * Revision 1.2  1995/09/05  21:22:05  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in conditional.prot */
/* file    : conditional.c
 * contents: conditional(), dissect_conditional()
 * purpose : implemetation of conditionals in RPN
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"
#include <ctype.h>

/* routine: conditional()
 * purpose: implement conditionals.  This is where the actual branching
 *          occurs in response to a conditional in the user's code.
 */

void conditional(void)
{
    char *branch;
    long is_true;

    branch = NULL;

    if (!stack_test(lstackptr, 1, "logical", "conditional")) {
        stop();
        rpn_set_error();
        return;
        }
    is_true = logicstack[lstackptr-1];
    lstackptr--;

#ifdef DEBUG
    fprintf(stderr, "conditional statement: %s\n", original=code_ptr->text+code_ptr->position);
    fflush(stdout);
#endif

    if (!dissect_conditional(&branch, is_true)) {
        fputs("error: improperly formed conditional\n", stderr);
        stop();
        rpn_set_error();
        return;
        }

#ifdef DEBUG
    fprintf(stderr, "branching to code: %s\n", branch);
    fprintf(stderr, "remainder is: %s\n",
            code_ptr->text+code_ptr->position);
    fprintf(stderr, "original is: %s\n", original);
    fflush(stdout);
#endif
    if (is_blank(code_ptr->text+code_ptr->position) && code_ptr->pred)
        pop_code();
    push_code(branch, VOLATILE);
#ifdef DEBUG
    fprintf(stderr, "current code is: %s\n", code_ptr->text);
    fflush(stdout);
#endif
    }

/* routine: dissect_conditional()
 * purpose: find the two branches of a conditional statement and return
 *          one of them, depending on what the branch condition is.
 *          Modifies the code to eliminate the entire conditional.
 */

long dissect_conditional(char **branch, long is_true)
{
    long n_ques, quote_count, space=0;
    char *colon, *dollar, *ptr, *ptr2;
    struct CODE *cptr;

    n_ques=1;
    colon = dollar = NULL;
    quote_count=0;

    cptr = code_ptr;
    ptr = cptr->text + cptr->position;
    if (*ptr=='#') {/* skip ending pcode symbol */
        ptr++;
        cptr->position++;
        }

    while (*ptr) {
        switch (*ptr) {
            case '?':
	        if (quote_count==0) 
                    n_ques++;
                break;
            case ':':
              if ((quote_count==0) && (space == 1)) {
                if (n_ques==1) {
                  ptr2 = ptr;
                  ptr2++;
                  if ((ptr2) && (*ptr2 == ' ')) {
                    colon = ptr;
                  }
                }
              }
              break;
            case '$':
	        if (quote_count==0) {
	            if (n_ques==1)
                        dollar = ptr;
                    else {
                        n_ques--;
                        if (n_ques<0)
                            bomb("n_ques<0", NULL);
		        }
		    }
                break;
	    case '"':
	        if (quote_count==0) 
	            quote_count++;
	        else
	            quote_count--;
	        break;
	    }
        if (*ptr == ' ')
          space = 1;
        else 
          space = 0;
        if (dollar!=NULL)
            break;
        ptr++;
        }
    if (dollar==NULL || colon==NULL)
        return(0);

    if (is_true) {
        /* copy portion up to colon into branch */
        *colon = 0;

        cp_str(branch, cptr->text + cptr->position);
        *colon = ':';      /* don't change source */
        }
    else {
        /* copy portion from colon to dollar sign into branch */
        *dollar = 0;
        cp_str(branch, colon+1);
        *dollar = '$';
        }

    /* update source to show that conditional has been executed */
    cptr->position = dollar+1 - cptr->text;

    return(1);
    }

void conditional_udf(long udf_current_step)
{
  long is_true;
  if (!stack_test(lstackptr, 1, "logical", "conditional_udf")) {
    stop();
    rpn_set_error();
    return;
  }
  is_true = logicstack[lstackptr-1];
  lstackptr--;
  if (is_true) {
    udf_id_createarray(udf_current_step + 1, udf_cond_stack[udf_stack[udf_current_step].index].cond_colon);
  }
  else {
    udf_id_createarray(udf_cond_stack[udf_stack[udf_current_step].index].cond_colon + 1, udf_cond_stack[udf_stack[udf_current_step].index].cond_dollar);
  }
  return;
}
  


