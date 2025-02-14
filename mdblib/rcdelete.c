/**
 * @file rcdelete.c
 * @brief Provides the rcdelete function for string manipulation.
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
 * @brief Deletes from a string every instance of any character between two specified ASCII characters, inclusive.
 *
 * This function iterates through the input string and removes any character that falls within the specified range [c0, c1] in the ASCII table.
 *
 * @param s The input string to be modified.
 * @param c0 The lower bound character in the ASCII range.
 * @param c1 The upper bound character in the ASCII range.
 * @return The modified string with specified characters removed.
 */
char *rcdelete(char *s, char c0, char c1) {
  register char *ptr0, *ptr1;

  ptr0 = ptr1 = s;
  while (*ptr1) {
    if (*ptr1 < c0 || *ptr1 > c1)
      *ptr0++ = *ptr1++;
    else
      ptr1++;
  }
  *ptr0 = 0;
  return (s);
}
