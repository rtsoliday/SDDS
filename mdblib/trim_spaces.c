/**
 * @file trim_spaces.c
 * @brief Implementation of trim_spaces function..
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C Saunders, R. Soliday
 */

#include "mdb.h"

/**
 * @brief Trims leading and trailing spaces from a string.
 *
 * This function removes all leading and trailing space characters from the
 * input string `s` by modifying it in place. It returns a pointer to the
 * trimmed string.
 *
 * @param s The string to be trimmed.
 * @return A pointer to the trimmed string.
 */
char *trim_spaces(char *s) {
  char *ptr;
  if (strlen(s) == 0)
    return (s);
  ptr = s;
  while (*ptr == ' ')
    ptr++;
  if (ptr != s)
    strcpy_ss(s, ptr);
  ptr = s + strlen(s) - 1;
  while (*ptr == ' ' && ptr != s)
    ptr--;
  *++ptr = 0;
  return (s);
}
