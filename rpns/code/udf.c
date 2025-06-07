/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in udf.prot */
/* file    : udf.c
 * contents: is_udf(), make_udf(), insert_udf(), revudf()
 * purpose : routines related to RPN user-defined functions
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"
#include <ctype.h>

long *udf_index = NULL;

int compare_udf_names(const void *u1, const void *u2)
{
    return strcmp( ((struct UDF *)u1)->udf_name, ((struct UDF *)u2)->udf_name );
    }

long find_udf(char *udf_name)
/* returns udf number---i.e., position in udf_index array that gives index in udf_list array */
{
  register long i;
  static struct UDF udf0;
  
  if (num_udfs==0)
    return -1;
  udf0.udf_name = udf_name;
  i = binaryIndexSearch((void**)udf_list, num_udfs, (void*)&udf0, compare_udf_names, 0);
  if (i<0)
    return -1;
  return udf_list[i]->udf_num;
}

long find_udf_mod(char *udf_name)
/* returns udf number---i.e., position in udf_index array that gives index in udf_list array */
{
  register long i;
  static struct UDF udf0;
  
  if (num_udfs==0)
    return -1;
  udf0.udf_name = udf_name;
  i = binaryIndexSearch((void**)udf_list, num_udfs, (void*)&udf0, compare_udf_names, 0);
  if (i<0)
    return -1;
  return i;
}

short get_udf(long number)
/* returns udf string from udf number */
{
  register long i;
  register struct UDF *udfptr;
  
  if (number<0 || number>=num_udfs)
    return(0);
  i = udf_index[number];  /* translate into index in sorted list */
  if (i<0 || i>=num_udfs) 
    bomb("invalid udf_list index", NULL);
  udfptr = udf_list[i];
  udf_id_createarray(udfptr->start_index, udfptr->end_index);
  return(1);
}

void get_udf_indexes(long number)
/* returns udf string from udf number */
{
  register long i;
  register struct UDF *udfptr;
  
  i = udf_index[number];  /* translate into index in sorted list */
  udfptr = udf_list[i];
  udf_id_createarray(udfptr->start_index, udfptr->end_index);
}

short is_udf(char *string)
{
  long i;
  if ((i=find_udf(string))<0)
    return(0);
  return get_udf(i);
}

void make_udf(void)
{
  static char name[20];
  static char function[2048];
  char *ptr, *dummy1=NULL;
  short is_string=0;
  double dummy;
  
  udf_changed = 1;
  
  if (istackptr==1)
    queryn("function name: ", name, 20);
  else {
    if (!fgets(name, 20, input_stack[istackptr-1].fp))
      return;
    chop_nl(name);
    if (input_stack[istackptr-1].filemode==ECHO)
      puts(name);
  }
  delete_chars(name, " ");
  if (strlen(name)==0)
    return;
  
  if (is_memory(&dummy, &dummy1, &is_string, name)!=-1) {
    fprintf(stderr, "can't create UDF with name %s--already in use as a memory\n", name);
    return;
  }
  if (is_func(name)!=-1) {
    fprintf(stderr, "can't create UDF with name %s--already in use as a keyword\n", name);
    return;
  }
  
  if (istackptr==1)
    puts("enter function (end with blank line)");
  ptr = function;
  while (fgets(ptr, 2048, input_stack[istackptr-1].fp)) {
    if (*ptr=='\n') {
      *ptr=0;
      break;
    }
    if (input_stack[istackptr-1].filemode==ECHO && istackptr!=1)
      fprintf(stderr, "%s", ptr);
    ptr += strlen(ptr);
  }
  if (*(ptr-1)=='\n')
    *(ptr-1) = 0;

  create_udf(name, function);
}

void create_udf(char *name, char *function)
{
  char *ptr;
  long i_udf, i;
  int32_t duplicate;
  static struct UDF udf0;
  
  if (num_udfs>=max_udfs) {
    udf_list = trealloc(udf_list, sizeof(*udf_list)*(max_udfs=num_udfs+100));
    udf_index = trealloc(udf_index, sizeof(*udf_index)*max_udfs);
  }
  
  udf0.udf_name = name;
  i_udf = binaryInsert((void**)udf_list, num_udfs, (void*)&udf0, compare_udf_names, &duplicate);
  if (!duplicate) {
    udf_list[i_udf] = tmalloc(sizeof(struct UDF));
    cp_str(&udf_list[i_udf]->udf_name, name);
    cp_str(&udf_list[i_udf]->udf_string, function);
    udf_list[i_udf]->udf_num = num_udfs;
    num_udfs++;
  }
  else {
    free(udf_list[i_udf]->udf_string);
    cp_str(&udf_list[i_udf]->udf_string, function);
  }
  cp_str(&ptr, function);
  gen_pcode(ptr, i_udf);
  for (i=0; i<num_udfs; i++)
    udf_index[udf_list[i]->udf_num] = i;
  free(ptr);
}

void rpn_mudf(void)
{
  char *name, *string;
  if ((string=pop_string())==NULL) {
    fputs("string stack empty (mudf)\n", stderr);
    stop();
    rpn_set_error();
    return;
  }
  if ((name=pop_string())==NULL) {
    fputs("string stack has too few items (mudf)\n", stderr);
    stop();
    rpn_set_error();
    return;
  }
  create_udf(name, string);
  link_udfs();
}


#define SBUFLEN 16384

void link_udfs(void)
{
  register long i;
  long num;
  char *dummy1=NULL;
  short is_string=0;
  double dummy;
  i = 0;
  while (i<=udf_unknownptr) {
    if ((num=find_udf(udf_unknown[i].keyword))!=-1) {
      /* This token is a udf name. */
      udf_modarray(2,num,0.0,udf_unknown[i].index); 
      udf_unknown[i] = udf_unknown[udf_unknownptr--];
    }
    else if ((num=is_memory(&dummy, &dummy1, &is_string, udf_unknown[i].keyword))!=-1) {
      /* This token is a memory recall operation. */
      if (is_string)
        udf_modarray(9,num,0.0,udf_unknown[i].index); 
      else
        udf_modarray(4,num,0.0,udf_unknown[i].index); 
      udf_unknown[i] = udf_unknown[udf_unknownptr--];
    }
    else {
      i++;
    }
  }
}

/*********************
void insert_udf(char *instr, char *udf_string)
{
    long ls, li;
    char *ptr;

    li = strlen(instr);
    ls = strlen(udf_string);

    if ((ls+li)>L_INSTRUCT*.8)
        fputs("warning: instruction list size > .8 max\n", stderr);
    if ((ls+li-1)>L_INSTRUCT) {
        fputs("memory exhausted\n", stderr);
        fprintf(stderr, "li = %ld\tls=%ld\n", li, ls);
        fputs("hit return to see instruction list\n", stderr);
        getchar();
        fprintf(stderr, "instruction list: %s\n", instr);
        bomb("memory exhausted (insert_udf)", NULL);
        }
    strcpy(instr+ls+1, instr);
    strcpy(instr, udf_string);
    *(instr+ls)=' ';

    ptr = instr+strlen(instr)-1;
    while (isspace(*ptr) && ptr!=instr)
        *ptr-- = 0;
    }
****************/

void revudf(void)
{
    struct UDF *udfptr;
    long i_udf;

    for (i_udf=0; i_udf<num_udfs; i_udf++) {
        udfptr = udf_list[i_udf];
        if (udfptr->udf_string==NULL || udfptr->udf_name==NULL)
            return;
        fprintf(stderr, "%s:\t%s\n", udfptr->udf_name, udfptr->udf_string);
        }
    }



