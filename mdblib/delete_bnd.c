/**
 * @file delete_bnd.c
 * @brief Implementation of string manipulation routine.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include <ctype.h>

/**
 * @brief Deletes bounding characters from a string.
 *
 * Removes all leading and trailing characters from the string `s` that are present in the string `t`.
 *
 * @param s The string to be modified.
 * @param t The string containing bounding characters to remove from `s`.
 * @return The modified string `s`.
 */
char *delete_bounding(s, t) char *s, *t;
{
  register char *ptr1, *ptr0, *ptrt;

  if (!s)
    return (s);
  ptr0 = s;
  while (*ptr0) {
    ptrt = t;
    while (*ptrt && *ptrt != *ptr0)
      ptrt++;
    if (*ptrt == *ptr0)
      ptr0++;
    else
      break;
  }

  ptr1 = ptr0 + strlen(ptr0) - 1;
  while (ptr1 != ptr0) {
    ptrt = t;
    while (*ptrt && *ptrt != *ptr1)
      ptrt++;
    if (*ptrt == *ptr1)
      ptr1--;
    else
      break;
  }

  *++ptr1 = 0;
  strcpy_ss(s, ptr0);
  return (s);
}
