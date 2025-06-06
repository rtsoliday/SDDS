/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in pcode.prot */
/* file    : pcode.c
 * contents: gen_pcode()
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"
#include <ctype.h>

#define BUFLEN 16384
#define PC_BUFLEN 16384
char bufferRPN[BUFLEN];
char pc_buf[PC_BUFLEN];

/* routine: gen_pcode()
 * purpose: converts a text string into pseudo-code, which is really just a
 *          short-hand way of writing the commands.  This pcode can be parsed
 *          much faster than text.
 */

void gen_pcode(char *s0, long i_udf)
{
  register long i, store, sstore, mem_num;
  register char *ptr;
  double dummy2;
  char *s, *dummy3=NULL;
  short is_string=0;
  long scan_pos, num;
  double x;
  
  cp_str(&s, s0);    /* don't want to corrupt caller's data */
  
#ifdef DEBUG
  fprintf(stderr, "udf: %s\n", s);
#endif
  
  /* find number for store command 'sto', since it must be treated
   * differently
   */
  store = -1;
  for (i=0; i<NFUNCS; i++) {
    if (strcmp("sto", funcRPN[i].keyword)==0) {
      store = i;
      break;
    }
  }
  sstore = -1;
  for (i=0; i<NFUNCS; i++) {
    if (strcmp("ssto", funcRPN[i].keyword)==0) {
      sstore = i;
      break;
    }
  }
  scan_pos = 0;
  udf_list[i_udf]->start_index = udf_stackptr;
    while ((ptr=get_token_rpn(s, bufferRPN, BUFLEN, &scan_pos))) {
      /* ptr points to the current token from the string */
      for (i=0; i<NFUNCS; i++) {
        /* check to see if the token is a built-in function */
        if (strcmp(ptr, funcRPN[i].keyword)==0) {
          /* token is a built-in function */
          if (funcRPN[i].keyword[0]=='?') {
            udf_createarray(5,0,0.0,ptr,i_udf);
            break;
          }
          if (i==store) {
            /* treat memory sto store operations differently, since
             * the memory name follows in the text string        */
            if (!(ptr=get_token_rpn(s, bufferRPN, BUFLEN, &scan_pos))) {
              fputs("error: sto requires memory name (gen_pcode)\n", stderr);
              fprintf(stderr, "error detected parsing string %s\n", s);
              stop();
              rpn_set_error();
              return;
            }
            if ((mem_num=is_memory(&dummy2, &dummy3, &is_string, ptr))==-1)
              mem_num = rpn_create_mem(ptr, 0);
            udf_createarray(3,mem_num,0.0,ptr,0);
            break;
          }
          if (i==sstore) {
            /* treat memory ssto store operations differently, since
             * the memory name follows in the text string        */
            if (!(ptr=get_token_rpn(s, bufferRPN, BUFLEN, &scan_pos))) {
              fputs("error: ssto requires memory name (gen_pcode)\n", stderr);
              fprintf(stderr, "error detected parsing string %s\n", s);
              stop();
              rpn_set_error();
              return;
            }
            if ((mem_num=is_memory(&dummy2, &dummy3, &is_string, ptr))==-1)
              mem_num = rpn_create_mem(ptr, 1);
            udf_createarray(8,mem_num,0.0,ptr,0);
            break;
          }
          /* start or continue pcode by adding code for this function */
          udf_createarray(1,i,0.0,ptr,0);
          break;
        }
      }
      if (i==NFUNCS) {
        /* token is not a (pcodeable) built-in function */
        if ((mem_num=is_memory(&dummy2, &dummy3, &is_string, ptr))!=-1) {
          /* Token is a memory name--i.e., a memory recall operation.
           * Start or continue pcode sequence with recall code for the
           * memory in question.
           4 - recall for num; 9 - recall for string
           */
          if (is_string)
            udf_createarray(9,mem_num,0.0,ptr,0);	
          else
            udf_createarray(4,mem_num,0.0,ptr,0);		
        }
        else if ((num=find_udf(ptr))!=-1) {
          /* ptr is a user defined function */
          udf_createarray(2,num,0.0,ptr,0);
        }
        else {
          switch (*ptr) {
          case '"':
            /* token is a string */
            udf_createarray(-1,0,0.0,ptr,0);
            break;
          case ':':
            /* token is colon in condition */
            udf_createarray(6,0,0.0,ptr,i_udf);
            break;
          case '$':
            /* token is end of conditional statment */
            udf_createarray(7,0,0.0,ptr,i_udf);
            break;
          default:
            if (!isdigit(*ptr) && *ptr!='-' && *ptr!='+' && *ptr!='.') {
              /* token is unknown */
              udf_createarray(-2,0,0.0,ptr,0);
            }
            else {
              if (get_double(&x, ptr)) {
                /* token is a number */
                udf_createarray(0,0,x,ptr,0);
              }
              else {
                /* token is unknown */
                udf_createarray(-2,0,0.0,ptr,0);
              }
            }
          }
        }
      }
    }
  udf_list[i_udf]->end_index = udf_stackptr;
  
#ifdef DEBUG
  fprintf(stderr, "pcode: %s\n", bptr);
#endif
  free(s);
  
}


