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
 Revision 1.17  2005/01/12 21:49:31  shang
 added is_string argument to is_memeory function to tell the memory data type

 Revision 1.16  2005/01/10 23:35:31  shang
 it now does not push the string back to string stack after strcmp with variables

 Revision 1.15  2005/01/10 21:07:36  shang
 modified to use the update functions

 Revision 1.14  2003/08/28 20:49:11  soliday
 Cleaned up the code.

 Revision 1.13  2003/03/17 22:19:01  borland
 Fixed bugs in previous changes. Needs to be checked for memory leaks
 in new code.

 Revision 1.12  2002/08/14 16:28:57  soliday
 Added Open License

 Revision 1.11  2000/06/15 15:52:51  soliday
 Renamed help to rpn_help so that it does not conflict with the IOC.

 Revision 1.10  2000/04/13 16:21:20  soliday
 Moved some WIN32 stuff to mdb.h

 Revision 1.9  2000/03/27 20:29:02  borland
 Added rpn_execn() and rpn_execs() functions and user functions execn and
 execs.  These execute commands from the string stack.  Results are put
 on the numerical or string stack.

 Revision 1.8  1999/12/15 19:01:23  soliday
 Added support for vxWorks.

 Revision 1.7  1999/09/14 18:21:37  soliday
 report_stats is no longer defined locally

 Revision 1.6  1999/07/08 22:22:19  soliday
 Added definition for sleep function

 Revision 1.5  1999/06/01 14:37:48  soliday
 Removed warnings when compiled under Linux.

 Revision 1.4  1998/08/11 18:27:52  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.3  1995/10/07  22:45:12  borland
 * Modified UDF code to use a sorted list plus an index.  Unfortunately, this
 * doesn't seem to have improved performance.
 *
 * Revision 1.2  1995/09/05  21:22:07  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in execute.prot */
/* file: execute.c
 * contents: execute_code(), is_func(), quit(), help(), stop(), ttrace(),
 *           stack_test(), rep_stats()
 *
 * Michael Borland
 */
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <ctype.h>
#include "rpn_internal.h"

#if defined(vxWorks)
#include <time.h>
#endif

/* routine: execute_code()
 * purpose: handles execution of code in stack of CODE structures. On
 *          calling, there is only one structure on the stack, which
 *          corresponds to a command in a file (or from the terminal).
 *          This is the subroutine that does the actual calls to the
 *          routines that implement the functions the user has access to.
 *          It also interprets "pcode".
 */

long execute_code(void)
{
  register char *ptr=NULL;
  register long index;
  char *text, *buffer, *token, *dummy=NULL;
  short is_string=0;
  double x;
  long return_code;
  
  return_code = -1;
  while (code_lev!=1 || !is_blank(code_ptr->text+code_ptr->position)) {
    /* Set some char pointers to point to the text, buffer, and
     * next token for the current CODE structure. */
    set_ptrs(&text, &buffer, &token);
    while (token!=NULL || (ptr=get_token_rpn(text, buffer, LBUFFER,
                                             &(code_ptr->position))) ) {
      /* If token!=NULL, return to parsing a partially parsed token,
       * which is a pcode-string.  Otherwise, get a new token from
       * the code. */
      if (token)
        ptr = token;
#ifdef DEBUG
      fprintf(stderr, "code_lev = %ld\tposition = %ld\n",
              code_lev, code_ptr->position);
      fprintf(stderr, "text = %s\nptr = %s\n", text, ptr);
      fflush(stdout);
      if (token!=NULL)
        fprintf(stderr, "token = %s\n", token);
#endif
      /* If this is a null token, continue scanning code. */
      if (*ptr==0) {
        token = code_ptr->token = NULL;
        continue;
      }
      if (*ptr=='"') {
        /* token is a text string for the string stack */
        push_string(ptr);
        continue;
      }
      if (is_udf(ptr)) {
        /* token is a udf name */
        if (do_trace)
          fprintf(stderr, "calling udf %s   %ld %ld %ld %ld %ld\n", ptr,
                            stackptr, sstackptr, lstackptr, astackptr, code_lev);
        return_code=cycle_through_udf();
        continue;
      }
      if (is_memory(&x, &dummy, &is_string, ptr)!=-1) {
        /* token is a memory name */
#ifdef DEBUG
        fprintf(stderr, "recalling memory %s\n", ptr); fflush(stdout);
#endif
        if (dummy) {
          sstack[sstackptr++] = dummy; 
          return_code = LOGICAL_FUNC;
        } else {
          stack[stackptr++] = x;
          return_code = NUMERIC_FUNC;
        }
        continue;
      }
      if ((index=is_func(ptr))!=-1) {
        /* token is a built-in function name */
        if (do_trace)
          fprintf(stderr, "calling %s\n", ptr);
        return_code = funcRPN[index].type;
        code_ptr->token = NULL;
                (*(funcRPN[index]).fn)();
        set_ptrs(&text, &buffer, &token);
        code_ptr->token = NULL;
        continue;
      }
      /* assume that token is either a number or an error */
      if (!isdigit(*ptr) && *ptr!='-' && *ptr!='+' && *ptr!='.') {
        fprintf(stderr, "unknown token: %s\n", ptr);
        rpn_set_error();
      }
      else {
        if (get_double(&x, ptr)==0) {
          fprintf(stderr, "unknown token: %s\n", ptr);
          rpn_set_error();
        }
        else {
          if (stackptr>=STACKSIZE)
            fprintf(stderr, "numeric stack overflow--number not pushed\n");
          else
            stack[stackptr++] = x;
        }
      }
    }
    pop_code();
  }
  return(return_code);
}

/* routine: set_ptrs()
 * purpose: set pointers used by execute_code() to keep track of what
 *          is being parsed.
 */

void set_ptrs(char **text, char **buffer, char **token)
{
  if (!code_ptr)
    bomb("code_ptr is NULL in set_ptrs()", NULL);
  if (!(*text = code_ptr->text))
    bomb("text pointer is NULL in set_ptrs()", NULL);
  *token = code_ptr->token;
  *buffer = code_ptr->buffer;
  if (*buffer==NULL)
    *buffer = tmalloc(sizeof(**buffer)*LBUFFER);
}

/* routine: is_func()
 * purpose: if the string is the name of a built-in function, return the
 *          number of that function in the array func, otherwise return -1.
 */

long is_func(char *string)
{
  long lo, hi, mid, cmp;
  
  lo = 0;
  hi = NFUNCS-1;
  while (hi-lo>1) {
    mid = (lo+hi)/2;
    if (!(cmp=strcmp(string, funcRPN[mid].keyword)))
      return(mid);
    if (cmp<0)
      hi = mid;
    else
      lo = mid;
  }
  if (!strcmp(string, funcRPN[lo].keyword))
    return(lo);
  if (!strcmp(string, funcRPN[hi].keyword))
    return(hi);
  return(-1);
}

/* routine: quit()
 * purpose: implement user's 'quit' command
 */

void quit(void)
{
  exit(1);
}

/* routine: help()
 * purpose: implement user's 'help' command
 */

void rpn_help(void)
{
  long i, l, lm;
  char s[200];
  
  lm = 0;
  for (i=0; i<NFUNCS; i++) {
    l = strlen(funcRPN[i].descrip)+strlen(funcRPN[i].keyword);
    if (l>lm)
      lm = l;
  }
  
  for (i=0; i<NFUNCS; i++) {
    l = strlen(funcRPN[i].descrip);
    strcpy(s, funcRPN[i].keyword);
    strncat(s, "                     ", lm-l-strlen(funcRPN[i].keyword));
    fprintf(stderr, "%s  %s\n", s, funcRPN[i].descrip);
  }
  
  if (i%2)
    fputc('\n', stderr);
  
  fputs(additional_help, stderr);
}

/* routine: stack_test()
 * purpose: test specified stack to see if it has a certain number of
 *          data elements.  If not, print an error message.
 */

long stack_test(long stack_ptr, long numneeded, char *stackname, char *caller)
{
  if (stack_ptr<numneeded) {
    fprintf(stderr, "too few items on %s stack (%s)\n", stackname, caller);
    return(0);
  }
  return(1);
}

/* routine: stop()
 * purpose: implement user's 'stop' command
 */

void stop(void)
{
  while (code_lev!=1)
    pop_code();
  fputs("*stop*\n", stderr);
}

/* routine: ttrace()
 * purpose: implement user's 'ttr' command
 */

void ttrace(void)
{
  do_trace = !do_trace;
}


/* routine: rep_stats()
 * purpose: report run-time statistics (user's command 'rs')
 */

void rep_stats(void)
{
  report_stats(stdout, " \0");
}


int func_compare(const void *f1, const void *f2)
{
  return(strcmp(((struct FUNCTION*)f1)->keyword,
                ((struct FUNCTION*)f2)->keyword));
}

void rpn_sleep(void)
{
  long seconds;
#if defined(vxWorks)
  struct timespec rqtp;
  rqtp.tv_nsec = 0;
#endif
  if ((seconds=pop_num())) {
#if defined(vxWorks)
    rqtp.tv_sec = 1;
    nanosleep(&rqtp,NULL);
#else
    sleep(seconds);
#endif
  }
}


long cycle_through_udf(void) 
{
  register long return_code, udf_current_step, udf_last_step;
  register int continue_cycle;
  register UDF_CODE udf_temp_stack;
  continue_cycle = 1;
  return_code = -1;
  do {
    udf_current_step = udf_id[cycle_counter].udf_start_index;
    udf_last_step = udf_id[cycle_counter].udf_end_index;
    while (udf_current_step < udf_last_step && continue_cycle) {
      udf_temp_stack = udf_stack[udf_current_step];
      switch (udf_temp_stack.type) {
      case -2:
        /* Unknown */
        fprintf(stderr, "unknown token: %s\n", udf_temp_stack.keyword);
        rpn_set_error();
        udf_current_step++;
        break;
      case -1:
        /* Text String */
        if (do_trace) 
          fprintf(stderr, "pushing %s onto string stack\n", udf_temp_stack.keyword);
        push_string(udf_temp_stack.keyword); 
        udf_current_step++;
        break;
      case 0:
        /* Number */
        if (stackptr>=STACKSIZE)
          fprintf(stderr, "numeric stack overflow--number not pushed\n");
        else {
          if (do_trace) 
            fprintf(stderr, "pushing %f onto numeric stack\n", udf_temp_stack.data);
          stack[stackptr++] = udf_temp_stack.data;
        }
        udf_current_step++;
        break;
      case 1:
        /* Built-in function */
        if (udf_temp_stack.index<0 || udf_temp_stack.index>NFUNCS) {
          fprintf(stderr, "pcode error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace)
          fprintf(stderr, "calling %s\n", udf_temp_stack.keyword);
        return_code = funcRPN[udf_temp_stack.index].type;
        (*(funcRPN[udf_temp_stack.index]).fn)();
        udf_current_step++;
        break;
      case 2:
        /* User-defined function */    
        if (udf_temp_stack.index<0 || udf_temp_stack.index>num_udfs) {
          fprintf(stderr, "pcode udf error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace)
          fprintf(stderr, "calling udf %s   %ld %ld %ld %ld %ld\n", udf_temp_stack.keyword, 
                  stackptr, sstackptr, lstackptr, astackptr, code_lev);
        udf_id[cycle_counter].udf_start_index = udf_current_step + 1;
        get_udf_indexes(udf_temp_stack.index);
        continue_cycle = 0;
        break;
      case 3:
        /* memory Store operation for sto*/
        if (udf_temp_stack.index<0 || udf_temp_stack.index>n_memories) {
          fprintf(stderr, "pcode store error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace) 
          fprintf(stderr, "memory store operation\n");
        push_num(memoryData[udf_temp_stack.index] = pop_num());
        udf_current_step++;
        break;
      case 4:
        /* memory Recall operation */
        if (udf_temp_stack.index<0 || udf_temp_stack.index>n_memories) {
          fprintf(stderr, "pcode recall error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace) 
          fprintf(stderr, "memory recall operation\n");
        return_code = NUMERIC_FUNC;
        push_num(memoryData[udf_temp_stack.index]);
        udf_current_step++;
        break;
      case 5:
        /* conditional operation */
        udf_id[cycle_counter].udf_start_index = udf_cond_stack[udf_temp_stack.index].cond_dollar + 1;
        return_code = 3;
        if (do_trace) 
          fprintf(stderr, "conditional operation\n");
        conditional_udf(udf_current_step);
        continue_cycle = 0;
        break;
      case 8:
        /* memory Store operation for ssto*/
        if (udf_temp_stack.index<0 || udf_temp_stack.index>n_memories) {
          fprintf(stderr, "pcode store error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace) 
          fprintf(stderr, "memory store operation\n");
        push_string(str_memoryData[udf_temp_stack.index] = pop_string());
        udf_current_step++;
        break;
      case 9:
        /* memory Recall operation for string */
        if (udf_temp_stack.index<0 || udf_temp_stack.index>n_memories) {
          fprintf(stderr, "pcode recall error: index is %d\n", udf_temp_stack.index);
          exit(1);
        }
        if (do_trace) 
          fprintf(stderr, "memory recall operation\n");
        return_code = LOGICAL_FUNC;
        push_string(str_memoryData[udf_temp_stack.index]); 
        udf_current_step++;
        break;
      default:
        fprintf(stderr, "unknown token: %s\n", udf_temp_stack.keyword);
        rpn_set_error();
        exit(1);
        break;
      }
    }
    if (continue_cycle) 
      cycle_counter = cycle_counter - 1;
      else
        continue_cycle = 1;
  }
  while  (cycle_counter != cycle_counter_stop); 
  return(return_code);
}


