/**
 * @file str_tolower.c
 * @brief Implementation of the str_tolower function.
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
 * @brief Convert a string to lower case.
 *
 * This function takes a string and converts all its characters to lower case.
 *
 * @param s The string to convert.
 * @return The converted string.
 */
char *str_tolower(s) char *s;
{
  register char *ptr;

  ptr = s;
  while (*ptr) {
    *ptr = tolower(*ptr);
    ptr++;
  }
  return (s);
}
