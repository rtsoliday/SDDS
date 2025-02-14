/**
 * @file str_inn.c
 * @brief Contains string str_inn function.
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
 * @brief Searches for the first occurrence of substring `t` within the first `n` characters of string `s`.
 *
 * This function scans the string `s` to find the substring `t`. It only considers the first `n` characters of `s`.
 * If `t` is found within these bounds, a pointer to the beginning of `t` in `s` is returned.
 *
 * @param s The string to be searched.
 * @param t The substring to locate within `s`.
 * @param n The maximum number of characters in `s` to be searched.
 * @return A pointer to the first occurrence of `t` in `s`, or `NULL` if `t` is not found within the first `n` characters.
 */
char *str_inn(s, t, n) char *s, *t;
long n;
{
  register char *ps0, *pt, *ps;
  register long i;

  if (s == NULL || t == NULL)
    return (NULL);

  ps0 = s;
  i = strlen(t);
  while (*ps0 && n >= i) {
    i++;
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
