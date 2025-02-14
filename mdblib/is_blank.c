/**
 * @file is_blank.c
 * @brief Implementation of is_blank function.
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
 * @brief Determine whether a string is composed entirely of whitespace characters.
 *
 * This function checks each character in the input string to verify if it is a space character.
 *
 * @param s The null-terminated string to be checked.
 * @return Returns 1 if the string is all whitespace, otherwise returns 0.
 */
long is_blank(char *s) {
  register char *ptr;

  ptr = s;
  while (*ptr && isspace(*ptr))
    ptr++;
  if (*ptr)
    return (0);
  return (1);
}
