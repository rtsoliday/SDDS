/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in memory.prot */
/* file    : memory.c
 * contents: rpn_create_mem(), rpn_store(), rpn_quick_store(), rpn_recall(),
 *           rpn_str_recall(), store_in_mem(), store_in_str_mem(),
 *           is_memory(), revmem()
 * purpose : RPN memory routines
 *
 * Michael Borland, 1988, 1992
 */
#include "rpn_internal.h"

/* routine: rpn_create_mem()
 * purpose: create a new memory with the name given
 */
long n_memories = 0, memory_added=0, max_n_memories = 0;

/* structure for memory index */
typedef struct {
  char *name;
  long index;
  short is_string;
} MEMORY ;


double *memoryData = NULL;
char **str_memoryData = NULL;
static MEMORY **Memory = NULL;

int compare_mem(const void *m1, const void *m2)
{
  return strcmp(((MEMORY *)m1)->name, ((MEMORY *)m2)->name);
}

long rpn_create_mem(char *name, short is_string)
{
  long i_mem;
  int32_t duplicate;
  MEMORY *newMem;
  
  if (is_func(name)!=-1 || find_udf(name)!=-1) {
    fprintf(stderr, "error: attempt to create rpn memory with reserved name \"%s\"\n", name);
    return -1;
  }
  
  if (Memory==NULL || n_memories>=max_n_memories) {
    Memory = trealloc(Memory, sizeof(*Memory)*(max_n_memories+=10));
    memoryData = trealloc(memoryData, sizeof(*memoryData)*max_n_memories);
    str_memoryData = trealloc(str_memoryData, sizeof(*str_memoryData)*max_n_memories);
  }
  
  newMem = tmalloc(sizeof(*newMem));
  newMem->name = name; /* temporary copy */
  
  i_mem = binaryInsert((void**)Memory, n_memories, (void*)newMem, compare_mem, &duplicate);
  if (duplicate) {
    free(newMem);
    return Memory[i_mem]->index;
  }
  
  cp_str(&newMem->name, name);
  newMem->index = n_memories;
  newMem->is_string = is_string;
  memoryData[n_memories] = 0;
  str_memoryData[n_memories] = NULL;
  n_memories++;
  memory_added = 1;
  return Memory[i_mem]->index;
}

/* routine: rpn_store()
 * purpose: utility command for fast storage to an rpn memory, by number
 */

long rpn_store(double value, char *str_value, long memory_number)
{
  if (memory_number>=0 && memory_number<n_memories) {
    str_memoryData[memory_number]=str_value;
    memoryData[memory_number] = value;
    return(1);
  }
  return(0);
}

long rpn_quick_store(double value, char *str_value, long memory_number)
{
  memoryData[memory_number] = value;
  str_memoryData[memory_number]=str_value;
  return 1;
}


/* routine: rpn_recall()
 * purpose: utility command for fast recall of an rpn memory, by number
 */

double rpn_recall(long memory_number)
{
  if (memory_number>=0 && memory_number<n_memories) {
    return memoryData[memory_number];
  }
  fputs("internal error: invalid memory number passed to rpn_recall()\n", stderr);
  return(0);
}

char *rpn_str_recall(long memory_number)
{
  if (memory_number>=0 && memory_number<n_memories) {
    char *ptr;
    cp_str(&ptr, str_memoryData[memory_number]);
    return ptr;
  }
  fputs("internal error: invalid memory number passed to rpn_str_recall()\n", stderr);
  return(0);
}


/* routine: store_in_str_mem()
 * purpose: implements user's 'ssto' command
 */

void store_in_mem(void)
{
  static long i_mem;
  static char *name;
  static char buffer[LBUFFER];
  
  if ((name = get_token_rpn(code_ptr->text,
                            buffer, LBUFFER, &(code_ptr->position)))==NULL) {
    fputs("store_in_mem syntax: sto name\n", stderr);
    stop();
    rpn_set_error();
    return;
        }
  
  if (stackptr<1) {
    fputs("sto requires value on stack\n", stderr);
    stop();
    rpn_set_error();
    return;
  }
  
  if ((i_mem = rpn_create_mem(name,0))>=0)
    memoryData[i_mem] = stack[stackptr-1];
}

/* routine: store_in_str_mem()
 * purpose: implements user's 'ssto' command
 */

void store_in_str_mem(void)
{
  static long i_mem;
  static char *name;
  static char buffer[LBUFFER];
  
  if ((name = get_token_rpn(code_ptr->text,
                            buffer, LBUFFER, &(code_ptr->position)))==NULL) {
    fputs("store_in_mem syntax: sto name\n", stderr);
    stop();
    rpn_set_error();
    return;
  }
  
  if (sstackptr<1) {
    fputs("ssto requires value on stack\n", stderr);
    stop();
    rpn_set_error();
    return;
  }
  
  if ((i_mem = rpn_create_mem(name,1))>=0)
    str_memoryData[i_mem] = sstack[sstackptr-1];
}

/* routine: is_memory()
 * purpose: return the memory number of a named memory, and put the
 *          data stored in the memory into a location supplied by the
 *          caller.  Basically an implementation of the memory recall
 *          facility.
 */

long is_memory(double *val, char **str_val, short *is_string,  char *string)
{
  long i_mem;
  MEMORY newMem;
  
  newMem.name = string;
  if ((i_mem=binaryIndexSearch((void**)Memory, n_memories, (void*)&newMem, compare_mem, 0))>=0) {
    *val = memoryData[Memory[i_mem]->index];
    if ((*is_string = Memory[i_mem]->is_string))
      cp_str(str_val, str_memoryData[Memory[i_mem]->index]);
    else
      *str_val = NULL;
    return Memory[i_mem]->index;
  }
  return(-1);
}

/* routine: revmem()
 * purpose: implement user's 'smem' command
 */

void revmem(void)
{
  long i_mem;
  double data;
  
  for (i_mem=0; i_mem<n_memories; i_mem++) {
    fprintf(stderr, "%s", Memory[i_mem]->name);
    if (Memory[i_mem]->is_string)
      fprintf(stderr,"\t %s\n", str_memoryData[Memory[i_mem]->index]);
    else {
      data = memoryData[Memory[i_mem]->index];
      fprintf(stderr, choose_format(format_flag, data), '\t', data, '\n');
    }
  }
}


