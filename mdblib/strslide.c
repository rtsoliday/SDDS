/**
 * @file strslide.c
 * @brief Implements the strslide function.
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
 * @brief Slides character data within a string by a specified distance.
 *
 * This function shifts the characters in the string `s` by the given `distance`.
 * A positive distance slides characters toward higher indices (to the right),
 * while a negative distance slides characters toward lower indices (to the left).
 * 
 * @param s Pointer to the null-terminated string to be modified.
 * @param distance The number of positions to slide the string. Positive values
 *                 shift characters to the right, and negative values shift
 *                 characters to the left.
 * @return Returns the modified string `s` on success. Returns `NULL` if the
 *         distance is greater than the string length when sliding to higher indices.
 */
char *strslide(char *s, long distance) {
  char *source, *target;
  long i, length;

  if (!s || !distance)
    return s;
  if (distance > 0) {
    /* slide toward higher index */
    source = s + (length = strlen(s));
    if (distance > length)
      return NULL;
    target = source + distance;
    for (i = length; i >= 0; i--)
      *target-- = *source--;
  } else if (distance < 0) {
    /* slide toward lower index */
    length = strlen(s);
    if ((distance = -distance) >= length)
      *s = 0;
    else {
      source = s + distance;
      target = s;
      do {
        *target++ = *source++;
      } while (*source);
      *target = 0;
    }
  }
  return s;
}
