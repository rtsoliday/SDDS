/**
 * @file replace_chars.c
 * @brief Provides functionality to map one set of characters into another within strings.
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
 * @brief Maps one set of characters to another in a given string.
 *
 * This function replaces each character in the string `s` that appears in the `from` string
 * with the corresponding character in the `to` string. If the `to` string is shorter than
 * the `from` string, it pads the `to` string with spaces. Similarly, if the `from` string
 * is shorter than the `to` string, it pads the `from` string with spaces.
 *
 * @param s The string to be modified.
 * @param from The set of characters to be replaced.
 * @param to The set of characters to replace with.
 * @return The modified string `s` with characters replaced.
 */
char *replace_chars(s, from, to) char *s, *from, *to;
{
  long lt, lf;
  char *ptr, *ptr_to, *ptr_from;

  if ((lt = strlen(to)) < (lf = strlen(from))) {
    char *to_temp;
    to_temp = tmalloc(sizeof(*to_temp) * (lf + 1));
    strcpy_ss(to_temp, to);
    for (; lt < lf; lt++)
      to_temp[lt] = ' ';
    to_temp[lf] = 0;
  } else if (lt > lf) {
    char *from_temp;
    from_temp = tmalloc(sizeof(*from_temp) * (lt + 1));
    strcpy_ss(from_temp, from);
    for (; lf < lt; lf++)
      from_temp[lf] = ' ';
    from_temp[lt] = 0;
  }

  ptr = s;
  while (*ptr) {
    ptr_to = to;
    ptr_from = from;
    while (*ptr_from)
      if (*ptr_from != *ptr) {
        ptr_from++;
        ptr_to++;
      } else {
        *ptr = *ptr_to;
        break;
      }
    ptr++;
  }
  return (s);
}
