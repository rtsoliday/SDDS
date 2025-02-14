/**
 * @file pad_with_spaces.c
 * @brief Provides functionality to manipulate and format strings.
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
 * @brief Adds a specified number of spaces to the end of a string.
 *
 * This function appends `n` spaces to the end of the input string `s`.
 *
 * @param s Pointer to the null-terminated string to be padded.
 * @param n The number of spaces to add to the end of the string.
 * @return Pointer to the padded string `s`.
 */
char *pad_with_spaces(char *s, int n) {
  char *ptr;

  ptr = s + strlen(s);
  while (n--)
    *ptr++ = ' ';
  *ptr = 0;
  return (s);
}
