/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


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
static long rpn_createarray_unlocked(long size)
{
    if (astackptr>=max_astackptr || !astack) 
        astack = trealloc(astack, sizeof(*astack)*(max_astackptr+=10));
    astack[astackptr].data = (double*)tmalloc(size*sizeof(double));
    astack[astackptr].rows = size;
    astackptr++;
    return astackptr-1;
    }

long rpn_createarray(long size)
{
    long index;

    rpn_lock();
    index = rpn_createarray_unlocked(size);
    rpn_unlock();
    return index;
    }

/* routine: rpn_resizearray 
 * purpose: resize an array
 */
static long rpn_resizearray_unlocked(long arraynum, long size)
{
    if (arraynum<0 || arraynum>=astackptr || !astack)
        return 0;
    astack[arraynum].data = (double*)trealloc(astack[arraynum].data, size*sizeof(double));
    astack[arraynum].rows = size;
    return 1;
    }

long rpn_resizearray(long arraynum, long size)
{
    long status;

    rpn_lock();
    status = rpn_resizearray_unlocked(arraynum, size);
    rpn_unlock();
    return status;
    }

/* routine: rpn_alloc
 * purpose: handle user's requests for array allocation
 */

static void rpn_alloc_unlocked(void)
{
    if (stackptr<1) {
        fputs("too few items on stack (_alloc)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }
    stack[stackptr-1] = rpn_createarray(stack[stackptr-1]);
    }

void rpn_alloc(void)
{
    rpn_lock();
    rpn_alloc_unlocked();
    rpn_unlock();
    }

/* routine: rref()
 * purpose: handle user's requests to Read by REFerence, i.e., to get
 *          an element from an array
 */

static void rref_unlocked(void)
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
    if (anum<0 || anum>=astackptr || !astack) {
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

void rref(void)
{
    rpn_lock();
    rref_unlocked();
    rpn_unlock();
    }


/* routine: sref()
 * purpose: handle user's requests to Store by REFerrence, i.e., to place
 *          a number in an array element.
 */

static void sref_unlocked(void)
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
    if (anum<0 || anum>=astackptr || !astack || ind<0 || ind>=astack[anum].rows) {
        fputs("access violation (sref)\n", stderr);
        stop();
        rpn_set_error();
        return;
        }
    astack[anum].data[ind] = stack[stackptr-3];
    stackptr -= 3;
    }

void sref(void)
{
    rpn_lock();
    sref_unlocked();
    rpn_unlock();
    }

/* routine: rpn_getarraypointer()
 * purpose: get the internal pointer used for array data.
 *
 */

static double *rpn_getarraypointer_unlocked(long memory_number, int32_t *length)
{
    long anum;

    if (length)
        *length = 0;
    if ((anum = rpn_recall(memory_number))<0 || anum>=astackptr || !astack)
        return NULL;
    if (length)
        *length = astack[anum].rows;
    return astack[anum].data;
    }

double *rpn_getarraypointer(long memory_number, int32_t *length)
{
    double *ptr;

    rpn_lock();
    ptr = rpn_getarraypointer_unlocked(memory_number, length);
    rpn_unlock();
    return ptr;
    }

/* routine: udf_createarray 
 * purpose: create a new array
 */
void udf_createarray_unlocked(long type, long index, double data, char *ptr, long i_udf)
{
    register long i, cond_temp, colon=0;
    if (udf_stackptr>=max_udf_stackptr || !udf_stack) 
        udf_stack = trealloc(udf_stack, sizeof(*udf_stack)*(max_udf_stackptr+=10));
    udf_stack[udf_stackptr].type = type;
    udf_stack[udf_stackptr].index = index;
    udf_stack[udf_stackptr].data = data;
    cp_str(&udf_stack[udf_stackptr].keyword,ptr);
    if (type==-2) {
        udf_create_unknown_array_unlocked(ptr,udf_stackptr);
        } 
    else if (type==7) {  
        cond_temp = 0;
	for (i=udf_stackptr - 1; i>=udf_list[i_udf]->start_index; i--) {
	    switch (udf_stack[i].type) {
	    case 5:
	      if (cond_temp==0) {
		  udf_cond_createarray_unlocked(colon,i);
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

void udf_createarray(long type, long index, double data, char *ptr, long i_udf)
{
    rpn_lock();
    udf_createarray_unlocked(type, index, data, ptr, i_udf);
    rpn_unlock();
    }

/* routine: udf_cond_createarray 
 * purpose: create a new array
 */
void udf_cond_createarray_unlocked(long colon, long i)
{
    if (udf_cond_stackptr>=max_udf_cond_stackptr || !udf_cond_stack) 
        udf_cond_stack = trealloc(udf_cond_stack, sizeof(*udf_cond_stack)*(max_udf_cond_stackptr+=4));
    udf_cond_stack[udf_cond_stackptr].cond_colon = colon;
    udf_cond_stack[udf_cond_stackptr].cond_dollar = udf_stackptr;
    udf_stack[i].index = udf_cond_stackptr;
    udf_cond_stackptr++;
    }

void udf_cond_createarray(long colon, long i)
{
    rpn_lock();
    udf_cond_createarray_unlocked(colon, i);
    rpn_unlock();
    }

/* routine: udf_modarray 
 * purpose: modify an existing udf array
 */
void udf_modarray_unlocked(long type, long index, double data, long i)
{
    udf_stack[i].type = type;
    udf_stack[i].index = index;
    udf_stack[i].data = data;
    }

void udf_modarray(long type, long index, double data, long i)
{
    rpn_lock();
    udf_modarray_unlocked(type, index, data, i);
    rpn_unlock();
    }

/* routine: udf_id_createarray 
 * purpose: create a new array
 */
void udf_id_createarray_unlocked(long start_index_value, long end_index_value)
{
    if (++cycle_counter>=max_cycle_counter || !udf_id) 
        udf_id = trealloc(udf_id, sizeof(*udf_id)*(max_cycle_counter+=100));
    udf_id[cycle_counter].udf_start_index = start_index_value;
    udf_id[cycle_counter].udf_end_index = end_index_value;
    }

void udf_id_createarray(long start_index_value, long end_index_value)
{
    rpn_lock();
    udf_id_createarray_unlocked(start_index_value, end_index_value);
    rpn_unlock();
    }

/* routine: udf_create_unknown_array
 * purpose: create an array containing the index and keyword of an unknown
 * unit in a udf
 */
void udf_create_unknown_array_unlocked(char *ptr, long index)
{
    if (++udf_unknownptr>=max_udf_unknown_counter || !udf_unknown)
        udf_unknown = trealloc(udf_unknown, sizeof(*udf_unknown)*(max_udf_unknown_counter+=4));
    udf_unknown[udf_unknownptr].index = index;
    cp_str(&udf_unknown[udf_unknownptr].keyword,ptr);
    }  

void udf_create_unknown_array(char *ptr, long index)
{
    rpn_lock();
    udf_create_unknown_array_unlocked(ptr, index);
    rpn_unlock();
    }
