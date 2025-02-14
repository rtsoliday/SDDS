/**
 * @file delete_chars.c
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
 * @brief Removes all occurrences of characters found in string `t` from string `s`.
 *
 * This function iterates through each character in `s` and removes it if it appears in `t`.
 *
 * @param s The input string from which characters will be removed. It is modified in place.
 * @param t The string containing characters to be removed from `s`.
 * @return A pointer to the modified string `s`.
 */
char *delete_chars(char *s, char *t) {
  char *ps, *pt;

  ps = s;
  while (*ps) {
    pt = t;
    while (*pt) {
      if (*pt == *ps) {
        strcpy_ss(ps, ps + 1);
        ps--;
        break;
      }
      pt++;
    }
    ps++;
  }
  return (s);
}
