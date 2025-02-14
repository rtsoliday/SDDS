/**
 * @file cp_str.c
 * @brief Implementation of string manipulation functions.
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

/**
 * @brief Copies a string, allocating memory for storage.
 *
 * This function duplicates the input string `t` by allocating sufficient memory
 * and copying its contents. If `t` is `NULL`, the destination pointer `s` is set to `NULL`.
 *
 * @param s Pointer to the destination string pointer.
 * @param t Source string to be copied.
 * @return The copied string, or `NULL` if `t` is `NULL`.
 */
char *cp_str(char **s, char *t) {
  if (t == NULL)
    *s = NULL;
  else {
    *s = tmalloc(sizeof(*t) * (strlen(t) + 1));
    strcpy_ss(*s, t);
  }
  return (*s);
}

/**
 * @brief Copies a specified number of characters from a string.
 *
 * This function duplicates the first `n` characters of the input string `t` by
 * allocating sufficient memory and copying the specified number of characters.
 * The copied string is null-terminated. If `t` is `NULL`, the destination pointer `s` is set to `NULL`.
 *
 * @param s Pointer to the destination string pointer.
 * @param t Source string from which to copy characters.
 * @param n Number of characters to copy.
 * @return The copied string with up to `n` characters, or `NULL` if `t` is `NULL`.
 */
char *cpn_str(char **s, char *t, long n) {
  if (t == NULL)
    *s = NULL;
  else {
    *s = tmalloc(sizeof(*t) * (n + 1));
    strncpy(*s, t, n);
    (*s)[n] = 0;
  }
  return (*s);
}
