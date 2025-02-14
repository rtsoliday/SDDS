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
 Revision 1.8  2003/08/28 20:49:10  soliday
 Cleaned up the code.

 Revision 1.7  2002/08/14 16:28:56  soliday
 Added Open License

 Revision 1.6  1999/06/01 14:37:15  soliday
 Removed warnings when compiled under Linux.

 Revision 1.5  1998/08/11 18:27:52  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 Revision 1.4  1997/12/19 17:03:51  borland
 Added better checking for problems with array stack.

 * Revision 1.3  1995/10/03  06:35:33  borland
 * Added better error message to rref (array.c).  Modified udf.c to allow more
 * than 1000 pcode ID numbers for built-ins, memories, etc; now consistent with
 * gen_pcode().
 *
 * Revision 1.2  1995/09/05  21:22:04  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in array.prot */
/* file    : array.c
 * contents: rpn_alloc(), rref(), sref()
 * purpose : implementation of user-controlled numerical arrays in RPN
 * notes: should check for array index within bounds
 *        should expand list of array numbers if necessary.
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"

/* routine: rpn_createarray 
 * purpose: create a new array
 */
long rpn_createarray(long size)
{
    if (astackptr>=max_astackptr || !astack) 
        astack = trealloc(astack, sizeof(*astack)*(max_astackptr+=10));
    astack[astackptr].data = (double*)tmalloc(size*sizeof(double));
    astack[astackptr].rows = size;
    astackptr++;
    return astackptr-1;
    }

/* routine: rpn_resizearray 
 * purpose: resize an array
 */
long rpn_resizearray(long arraynum, long size)
{
    if (arraynum>astackptr || (arraynum<0 && !astack))
        return 0;
    astack[arraynum].data = (double*)trealloc(astack[arraynum].data, size*sizeof(double));
    astack[arraynum].rows = size;
    return 1;
    }

/* routine: rpn_alloc
 * purpose: handle user's requests for array allocation
 */

void rpn_alloc(void)
{
    if (stackptr<1) {
        fputs("too few items on stack (_alloc)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }
    stack[stackptr-1] = rpn_createarray(stack[stackptr-1]);
    }

/* routine: rref()
 * purpose: handle user's requests to Read by REFerence, i.e., to get
 *          an element from an array
 */

void rref(void)
{
    long anum, ind;

    if (stackptr<2) {
        fputs("too few items on stack (rref)\n", stderr);
        fputs("rrf usage example: array_elem array_num rrf\n", stderr);
        fputs("(Recalls array_elem-th element of array_num-th array.)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }

    anum = stack[stackptr-1];
    ind = stack[stackptr-2];
    if (anum>astackptr) {
        fprintf(stderr, "array pointer %ld is invalid (rref)\n", anum);
        stop();
        rpn_set_error();
        return;
        }
    if (ind<0 || ind>=astack[anum].rows) {
        fprintf(stderr, "access violation for position %ld of array %ld (rref)\n",
                ind, anum);
        stop();
        rpn_set_error();
        return;
        }
    stack[stackptr-2] = astack[anum].data[ind];
    stackptr -= 1;
    }


/* routine: sref()
 * purpose: handle user's requests to Store by REFerrence, i.e., to place
 *          a number in an array element.
 */

void sref(void)
{
    long anum, ind;

    if (stackptr<3) {
        fputs("too few items on stack (sref)\n", stderr);
        fputs("srf usage example: number array_elem array_num srf\n", stderr);
        fputs("(Stores number in the array_elem-th element of the array_num-th array.)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }

    anum = stack[stackptr-1];
    ind = stack[stackptr-2];
    if (anum>astackptr || ind<0 || ind>=astack[anum].rows) {
        fputs("access violation (sref)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }
    astack[anum].data[ind] = stack[stackptr-3];
    stackptr -= 3;
    }

/* routine: rpn_getarraypointer()
 * purpose: get the internal pointer used for array data.
 *
 */

double *rpn_getarraypointer(long memory_number, int32_t *length)
{
    long anum;

    if ((anum = rpn_recall(memory_number))<0 || anum>astackptr)
        return NULL;
    *length = astack[anum].rows;
    return astack[anum].data;
    }

/* routine: udf_createarray 
 * purpose: create a new array
 */
void udf_createarray(short type, short index, double data, char *ptr, long i_udf)
{
    register long i, cond_temp, colon=0;
    if (udf_stackptr>=max_udf_stackptr || !udf_stack) 
        udf_stack = trealloc(udf_stack, sizeof(*udf_stack)*(max_udf_stackptr+=10));
    udf_stack[udf_stackptr].type = type;
    udf_stack[udf_stackptr].index = index;
    udf_stack[udf_stackptr].data = data;
    cp_str(&udf_stack[udf_stackptr].keyword,ptr);
    if (type==-2) {
        udf_create_unknown_array(ptr,udf_stackptr);
        } 
    else if (type==7) {  
        cond_temp = 0;
	for (i=udf_stackptr - 1; i>=udf_list[i_udf]->start_index; i--) {
	    switch (udf_stack[i].type) {
	    case 5:
	      if (cond_temp==0) {
		  udf_cond_createarray(colon,i);
		  i = udf_list[i_udf]->start_index;
		  break;
	          }
	      cond_temp--;
	      break;
	    case 6:
	      if (cond_temp==0) 
		  colon = i;
	      break;
	    case 7:
	      cond_temp++;
	      break;
	    default:
	      break;
	    }
	    }
        }
    udf_stackptr++;
    }

/* routine: udf_cond_createarray 
 * purpose: create a new array
 */
void udf_cond_createarray(long colon, long i)
{
    if (udf_cond_stackptr>=max_udf_cond_stackptr || !udf_cond_stack) 
        udf_cond_stack = trealloc(udf_cond_stack, sizeof(*udf_cond_stack)*(max_udf_cond_stackptr+=4));
    udf_cond_stack[udf_cond_stackptr].cond_colon = colon;
    udf_cond_stack[udf_cond_stackptr].cond_dollar = udf_stackptr;
    udf_stack[i].index = udf_cond_stackptr;
    udf_cond_stackptr++;
    }

/* routine: udf_modarray 
 * purpose: modify an existing udf array
 */
void udf_modarray(short type, short index, double data, long i)
{
    udf_stack[i].type = type;
    udf_stack[i].index = index;
    udf_stack[i].data = data;
    }

/* routine: udf_id_createarray 
 * purpose: create a new array
 */
void udf_id_createarray(long start_index_value, long end_index_value)
{
    if (++cycle_counter>=max_cycle_counter || !udf_id) 
        udf_id = trealloc(udf_id, sizeof(*udf_id)*(max_cycle_counter+=100));
    udf_id[cycle_counter].udf_start_index = start_index_value;
    udf_id[cycle_counter].udf_end_index = end_index_value;
    }

/* routine: udf_create_unknown_array
 * purpose: create an array containing the index and keyword of an unknown
 * unit in a udf
 */
void udf_create_unknown_array(char *ptr, long index)
{
    if (++udf_unknownptr>=max_udf_unknown_counter || !udf_unknown)
        udf_unknown = trealloc(udf_unknown, sizeof(*udf_unknown)*(max_udf_unknown_counter+=4));
    udf_unknown[udf_unknownptr].index = index;
    cp_str(&udf_unknown[udf_unknownptr].keyword,ptr);
    }  
