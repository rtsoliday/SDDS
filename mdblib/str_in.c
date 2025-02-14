/**
 * @file str_in.c
 * @brief Implementation of the str_in function.
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
 * @brief Finds the first occurrence of the substring `t` in the string `s`.
 *
 * This function searches for the substring `t` within the string `s` and returns a pointer
 * to the first occurrence of `t`. If `t` is not found, the function returns `NULL`.
 *
 * @param s The string to be searched.
 * @param t The substring to search for within `s`.
 * @return A pointer to the first occurrence of `t` in `s`, or `NULL` if `t` is not found.
 */
char *str_in(char *s, char *t) {
  register char *ps0, *pt, *ps;

  if (s == NULL || t == NULL)
    return (NULL);

  ps0 = s;
  while (*ps0) {
    if (*t == *ps0) {
      ps = ps0 + 1;
      pt = t + 1;
      while (*pt && *ps == *pt) {
        pt++;
        ps++;
      }
      if (*pt == 0)
        return (ps0);
    }
    ps0++;
  }
  return (NULL);
}
