/**
 * @file replace_string.c
 * @brief Provides functions to replace occurrences of substrings within strings.
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
 * @brief Replace all occurrences of one string with another string.
 *
 * This function replaces all instances of the substring `orig` in the source string `s` with the substring `repl`,
 * and stores the result in the destination string `t`.
 *
 * @param t Destination string where the result is stored.
 * @param s Source string in which replacements are to be made.
 * @param orig The substring to be replaced.
 * @param repl The substring to replace with.
 * @return The number of replacements made.
 */
int replace_string(char *t, char *s, char *orig, char *repl) {
  return replaceString(t, s, orig, repl, -1, 0);
}

/**
 * @brief Replace a limited number of occurrences of one string with another string.
 *
 * This function replaces up to `count_limit` instances of the substring `orig` in the source string `s` with the substring `repl`,
 * and stores the result in the destination string `t`.
 *
 * @param t Destination string where the result is stored.
 * @param s Source string in which replacements are to be made.
 * @param orig The substring to be replaced.
 * @param repl The substring to replace with.
 * @param count_limit The maximum number of replacements to perform.
 * @return The number of replacements made.
 */
int replace_stringn(char *t, char *s, char *orig, char *repl, long count_limit) {
  return replaceString(t, s, orig, repl, count_limit, 0);
}

/**
 * @brief Replace occurrences of one string with another string with additional options.
 *
 * This function replaces instances of the substring `orig` in the source string `s` with the substring `repl`,
 * up to a maximum of `count_limit` replacements. The `here` parameter controls additional replacement behavior.
 * The result is stored in the destination string `t`.
 *
 * @param t Destination string where the result is stored.
 * @param s Source string in which replacements are to be made.
 * @param orig The substring to be replaced.
 * @param repl The substring to replace with.
 * @param count_limit The maximum number of replacements to perform. If negative, no limit is applied.
 * @param here Additional parameter to control replacement behavior.
 * @return The number of replacements made.
 */
int replaceString(char *t, char *s, char *orig, char *repl, long count_limit, long here) {
  char *ptr0, *ptr1;
  int count;
  char temp;

  ptr0 = s;
  t[0] = 0;
  count = 0;
  while ((count_limit < 0 || count < count_limit) && (ptr1 = str_in(ptr0, orig))) {
    if (here && ptr1 != ptr0)
      break;
    count++;
    temp = *ptr1;
    *ptr1 = 0;
    strcat(t, ptr0);
    strcat(t, repl);
    ptr0 = ptr1 + strlen(orig);
    *ptr1 = temp;
  }
  if (strlen(ptr0))
    strcat(t, ptr0);
  return (count);
}
