/**
 * @file compress.c
 * @brief Provides a string manipulation function.
 *
 * This file contains functions to create, manage, and manipulate buffers that store
 * lines of text strings. Buffers can be dynamically created, added to, cleared, and
 * printed to files.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author C. Saunders, R. Soliday
 */

#include "mdb.h"
#include <ctype.h>

/**
 * @brief Eliminates repeated occurrences of characters in string t from string s.
 *
 * This function removes consecutive duplicate characters in string `s` that are present in string `t`.
 *
 * @param s Pointer to the string to be compressed. The string is modified in place.
 * @param t Pointer to the string containing characters to remove from `s`.
 * @return Pointer to the compressed string `s`.
 */
char *compressString(char *s, char *t) {
  register char *ptr, *ptr0, *tptr;

  ptr = ptr0 = s;
  while (*ptr0) {
    tptr = t;
    while (*tptr) {
      if (*tptr != *ptr0) {
        tptr++;
        continue;
      }
      while (*++ptr0 == *tptr)
        ;
      tptr++;
      ptr0--;
    }
    *ptr++ = *ptr0++;
  }
  *ptr = 0;
  return (s);
}
