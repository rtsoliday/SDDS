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
  Revision 1.13  2011/04/20 21:22:53  borland
  Replaced strcpy() by strcpy_ss() to fix bug on RHEL6.

  Revision 1.12  2010/02/04 14:39:22  borland
  Added processNamelist(), which is a new interface to the namelist processor
  that returns NAMELIST_ERROR when their is an error, instead of exiting.

  Revision 1.11  2008/03/18 22:03:35  borland
  Changed the special characters for RPN evaluation from [] to ().

  Revision 1.10  2008/02/21 18:41:48  soliday
  Fixed an issue with allocating memory for a variable I didn't have to.

  Revision 1.9  2007/10/22 20:50:38  soliday
  Added the ability to use RPN expressions in the namelist parser.

  Revision 1.8  2007/10/22 19:02:26  soliday
  Reindented code.

  Revision 1.7  2005/12/07 16:21:27  soliday
  Fixed some compiler warnings on Linux.

  Revision 1.6  2005/11/09 22:03:36  soliday
  Added support for int32_t

  Revision 1.5  2002/08/14 16:23:49  soliday
  Added Open License

  Revision 1.4  1999/12/15 16:43:22  soliday
  Added vxWorks support.

  Revision 1.3  1999/04/14 14:05:15  borland
  Works with new build system.  Fixed some missing prototypes and function type mismatches.

  Revision 1.2  1995/09/05 21:21:43  saunders
  First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "match_string.h"
#include "namelist.h"
#include "rpn.h"
#include <ctype.h>
#if !defined(vxWorks)
#include <memory.h>
#endif

/* routine: process_namelists()
 * purpose: process namelist input, putting a NAMELIST_TEXT structure
 *          into the correct namelist from the array
 */

long process_namelists(
                       NAMELIST **nl,
                       char **nl_name,
                       long n_nl,
                       NAMELIST_TEXT *nl_t
                       )
{
  long i_namelist;

  if ((i_namelist=match_string(nl_t->group_name, nl_name, n_nl, EXACT_MATCH))<0) {
    printf("error: unknown namelist %s referrenced\n", nl_t->group_name);
    exit(1);
  }
  return(process_namelist(nl[i_namelist], nl_t));
}

static long np_procflags = 0;

void set_namelist_processing_flags(long processing_flags)
{
  np_procflags = processing_flags;
}

/* routine: process_namelist()
 * purpose: process an array of namelist items
 */

long process_namelist(NAMELIST *nl, NAMELIST_TEXT *nl_t)
{
  long code;
  if ((code=processNamelist(nl, nl_t))<0)
    exit(1);
  return code;
}


long processNamelist(NAMELIST *nl, NAMELIST_TEXT *nl_t)
{
  register long i, j, k;
  char **item_name;
  long n_items;

  item_name = tmalloc(sizeof(*item_name)*(n_items=nl->n_items));
  for (i=0; i<n_items; i++) {
    item_name[i] = nl->item_list[i].name;
    if (np_procflags&STICKY_NAMELIST_DEFAULTS)
      memcpy(nl->item_list[i].root, nl->item_list[i].def_root, nl->item_list[i].data_size);
  }

  for (i=j=0; i<nl_t->n_entities; i++) {
    if ((k=process_entity(nl->item_list, item_name, n_items, nl_t, i))<0)
      return NAMELIST_ERROR;
    j += k;
  }
  
  free(item_name);
  return(j);
}


void reset_namelist_values(NAMELIST *nl)
{
  register long i;
  char **item_name;
  long n_items;

  item_name = tmalloc(sizeof(*item_name)*(n_items=nl->n_items));
  for (i=0; i<n_items; i++) {
    item_name[i] = nl->item_list[i].name;
    memcpy(nl->item_list[i].root, nl->item_list[i].def_root, nl->item_list[i].data_size);
  }
}

/* routine: process_entity()
 * purpose: process an entity from a namelist, putting the data into 
 *          the right memory locations.
 */

long process_entity(
                    ITEM *item,
                    char **item_name,
                    long n_items,
                    NAMELIST_TEXT *nl_t,
                    long i_entity
                    )
{
  register long i_item;
  register long value, repetition;
  short *ptr_short;
  long *ptr_long;
  int *ptr_int;
  int32_t *ptr_int32;
  double *ptr_double;
  float *ptr_float;
  char **ptr_string, *ptr_char;
  char *namelist_str=NULL;
  int len;
  
  if ((i_item = match_string(nl_t->entity[i_entity], item_name, n_items, 
                             EXACT_MATCH))<0) {
    printf("error: unknown variable %s used in namelist %s\n", 
           nl_t->entity[i_entity], nl_t->group_name);
    printf("known variables are: \n");
    for (i_item=0; i_item<n_items; i_item++) 
      printf("  %s\n", item_name[i_item]);
    return NAMELIST_ERROR;
  }
    
  if (nl_t->n_subscripts[i_entity]!=item[i_item].n_subscripts) {
    printf("error: wrong number of subscripts given for variable %s in namelist %s\n",
           nl_t->entity[i_entity], nl_t->group_name);
    return NAMELIST_ERROR;
  }
    
  switch (item[i_item].type) {
  case TYPE_SHORT:         /* short integer */
    if (!(ptr_short = (short *)get_address(item[i_item].root, 
                                           item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                           item[i_item].dimensions, sizeof(*ptr_short))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_short = (short)rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%hd", ptr_short)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_short++;
      }
    }
    break;
  case TYPE_INT:        /* integer */
    if (!(ptr_int = (int *)get_address(item[i_item].root, 
                                       item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                       item[i_item].dimensions, sizeof(*ptr_int))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_int = (int)rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%d", ptr_int)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_int++;
      }
    }
    break;
  case TYPE_INT32_T:        /* integer */
    if (!(ptr_int32 = (int32_t *)get_address(item[i_item].root, 
                                             item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                             item[i_item].dimensions, sizeof(*ptr_int32))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_int32 = (int32_t)rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%" SCNd32, ptr_int32)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_int32++;
      }
    }
    break;
  case TYPE_LONG:        /* long integer */
    if (!(ptr_long = (long *)get_address(item[i_item].root, 
                                         item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                         item[i_item].dimensions, sizeof(*ptr_long))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_long = (long)rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%ld", ptr_long)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_long++;
      }
    }
    break;
  case TYPE_FLOAT:        /* float */
    if (!(ptr_float = (float *)get_address(item[i_item].root, 
                                           item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                           item[i_item].dimensions, sizeof(*ptr_float))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_float = (float)rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%f", ptr_float)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_float++;
      }
    }
    break;
  case TYPE_DOUBLE:        /* double */
    if (!(ptr_double = (double *)get_address(item[i_item].root, 
                                             item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                             item[i_item].dimensions, sizeof(*ptr_double))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        len = strlen(nl_t->value[i_entity][value]);
        namelist_str = trim_spaces(nl_t->value[i_entity][value]);
        len = strlen(namelist_str);
        if ((len > 2) && 
            (namelist_str[0] == '(') && 
            (namelist_str[len - 1] == ')')) {
          fflush(stdout);
          strcpy_ss(namelist_str, namelist_str+1);
          namelist_str[len - 2] = 0;
          *ptr_double = rpn(namelist_str);
          if (rpn_check_error()) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
          rpn_clear();
        } else {
          if (1!=sscanf(nl_t->value[i_entity][value], "%lf", ptr_double)) {
            printf("error scanning value %s for namelist member %s\n",
                   nl_t->value[i_entity][value], item[i_item].name);
            return NAMELIST_ERROR;
          }
        }
        ptr_double++;
      }
    }
    break;
  case TYPE_STRING:      /* NUL-terminated character string */
    if (!(ptr_string = (char **)get_address(item[i_item].root, 
                                            item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                            item[i_item].dimensions, sizeof(*ptr_string))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        if (strcmp(nl_t->value[i_entity][value], "{NULL}")==0 || strcmp(nl_t->value[i_entity][value], "(NULL)")==0 ||
            strcmp(nl_t->value[i_entity][value], "{null}")==0 || strcmp(nl_t->value[i_entity][value], "(null)")==0)
          *ptr_string = NULL;
        else 
          cp_str(ptr_string, nl_t->value[i_entity][value]);
        ptr_string++;
      }
    }
    break;
  case TYPE_CHAR:         /* character */
    if (!(ptr_char = (char *)get_address(item[i_item].root, 
                                         item[i_item].n_subscripts, nl_t->subscript[i_entity], 
                                         item[i_item].dimensions, sizeof(*ptr_char))))
      return NAMELIST_ERROR;
    for (value=0; value<nl_t->n_values[i_entity]; value++) {
      for (repetition=0; repetition<nl_t->repeat[i_entity][value]; repetition++) {
        if (1!=sscanf(nl_t->value[i_entity][value], "%c", ptr_char)) {
          printf("error scanning value %s for namelist member %s\n",
                 nl_t->value[i_entity][value], item[i_item].name);
          return NAMELIST_ERROR;
        }
        ptr_char++;
      }
    }
    break;
  default:
    printf("unknown item type in process_item: %ld\n", item[i_item].type);
    return NAMELIST_ERROR;
    break;
  }
  return(1);
}
        
char *get_address(
                  char *root,          /* pointer to the first element of array */
                  long n_subs,         /* number of subscripts */
                  long *subscript,     /* array of subscripts */
                  long *dimension,     /* array of dimensions */
                  unsigned long size   /* size of array element and value in bytes */
                  )
{
  register char *ptr;
  register long i, offset;

  if (n_subs>0) {
    if (subscript[n_subs-1]>=dimension[n_subs-1]) {
      printf("subscript out of range in namelist variable\n");
      return NULL;
    }
    offset = subscript[n_subs-1];
    for (i=1; i<n_subs; i++) {
      if (subscript[i-1]>=dimension[i-1]) {
        printf("subscript out of range in namelist variable\n");
        return NULL;
      }
      offset += dimension[i]*subscript[i-1];
    }
  }
  else 
    offset = 0;

  ptr = root + size*offset;
  return(ptr);
}


