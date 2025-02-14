/**
 * @file interpret_escapes.c
 * @brief Provides functionality to interpret C escape sequences in strings.
 *
 * This file contains functions to interpret C-style escape sequences such as
 * \n (newline), \t (tab), and octal specifications like \ddd.
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
 * @brief Interpret C escape sequences in a string.
 *
 * This function processes the input string and replaces C-style escape sequences
 * (such as \n for newline, \t for tab, and octal sequences like \ddd) with their
 * corresponding characters.
 *
 * @param s Pointer to the string containing escape sequences to interpret.
 */
void interpret_escapes(char *s) {
  char *ptr;
  long count;

  ptr = s;
  while (*s) {
    if (*s == '"') {
      do {
        *ptr++ = *s++;
      } while (*s != '"' && *s);
      if (*s)
        *ptr++ = *s++;
    } else if (*s != '\\')
      *ptr++ = *s++;
    else {
      s++;
      if (!*s) {
        *ptr++ = '\\';
        *ptr++ = 0;
        return;
      }
      switch (*s) {
      case '\\':
        *ptr++ = '\\';
        s++;
        break;
      case 'n':
        *ptr++ = '\n';
        s++;
        break;
      case 't':
        *ptr++ = '\t';
        s++;
        break;
      default:
        if (*s >= '0' && *s <= '9') {
          *ptr = 0;
          count = 0;
          while (++count <= 3 && *s >= '0' && *s <= '9')
            *ptr = 8 * (*ptr) + *s++ - '0';
          ptr++;
        } else {
          *ptr++ = '\\';
        }
        break;
      }
    }
  }
  *ptr = 0;
}
