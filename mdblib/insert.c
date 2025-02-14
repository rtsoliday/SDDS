/**
 * @file insert.c
 * @brief Provides string manipulation function.
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
 * @brief Inserts a substring into a target string.
 *
 * This function inserts the string `t` into the string `s` at the beginning.
 * It is assumed that `s` has sufficient space to accommodate the additional characters.
 *
 * @param s Pointer to the destination string where `t` will be inserted.
 * @param t Pointer to the source string to be inserted into `s`.
 * @return Pointer to the modified string `s`.
 */
char *insert(s, t) char *s; /* pointer to character in string to insert at */
char *t;
{
  register long i, n;

  n = strlen(t);
  if (n == 0)
    return (s);

  for (i = strlen(s); i >= 0; i--) {
    s[i + n] = s[i];
  }

  for (i = 0; i < n; i++)
    s[i] = t[i];

  return (s);
}
