/**
 * @file match_string.c
 * @brief Provides functions for matching strings with various matching modes.
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

#include "match_string.h"
#include "mdb.h"
#include <ctype.h>

/**
 * @brief Matches a given string against an array of option strings based on specified modes.
 *
 * This function searches for a match of the input string within the provided array of option strings.
 * It supports different matching modes such as wildcard matching, case sensitivity, and whole string matching.
 * Depending on the mode flags, it can return the first match or indicate ambiguity.
 *
 * @param string The string to find a match for.
 * @param option The array of strings to match against.
 * @param n_options The number of option strings in the array.
 * @param mode Flags that determine the matching behavior (e.g., WILDCARD_MATCH, MATCH_WHOLE_STRING, CASE_SENSITIVE, RETURN_FIRST_MATCH).
 *
 * @return The index of the matching string in the options array, or -1 if no match is found or if multiple matches exist when ambiguity is not allowed.
 */
long match_string(
  char *string,   /* string to find match for */
  char **option,  /* strings to match with */
  long n_options, /* number of strings to match with */
  long mode       /* matching mode flags */
) {
  register long i, i_match, l;

  if (string == NULL)
    return (-1);

  if (mode & WILDCARD_MATCH) {
    for (i = 0; i < n_options; i++)
      if (wild_match(string, option[i]))
        return i;
    return -1;
  }

  if (!(mode & MATCH_WHOLE_STRING)) {
    l = strlen(string);
    i_match = -1;
    if (mode & CASE_SENSITIVE) {
      for (i = 0; i < n_options; i++) {
        if (strncmp(string, option[i], l) == 0) {
          if (mode & RETURN_FIRST_MATCH)
            return (i);
          if (i_match != -1)
            return (-1);
          i_match = i;
        }
      }
      return (i_match);
    } else {
      for (i = 0; i < n_options; i++) {
        if (strncmp_case_insensitive(string, option[i], MIN(l, (long)strlen(option[i]))) == 0) {
          if (mode & RETURN_FIRST_MATCH)
            return (i);
          if (i_match != -1)
            return (-1);
          i_match = i;
        }
      }
      return (i_match);
    }
  }

  if (mode & MATCH_WHOLE_STRING) {
    i_match = -1;
    if (mode & CASE_SENSITIVE) {
      for (i = 0; i < n_options; i++) {
        if (strcmp(string, option[i]) == 0) {
          if (mode & RETURN_FIRST_MATCH)
            return (i);
          if (i_match != -1)
            return (-1);
          i_match = i;
        }
      }
      return (i_match);
    } else {
      for (i = 0; i < n_options; i++) {
        if (strcmp_case_insensitive(string, option[i]) == 0) {
          if (mode & RETURN_FIRST_MATCH)
            return (i);
          if (i_match != -1)
            return (-1);
          i_match = i;
        }
      }
      return (i_match);
    }
  }

  /* unknown set of flags */
  puts("error: unknown flag combination in match_string()");
  puts("       contact programmer!");
  exit(1);
}

/**
 * @brief Compares two strings in a case-insensitive manner.
 *
 * This function compares two null-terminated strings without considering the case of the characters.
 * It returns an integer less than, equal to, or greater than zero if the first string is found,
 * respectively, to be less than, to match, or be greater than the second string.
 *
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 *
 * @return An integer indicating the relationship between the strings:
 *         - Less than zero if s1 is less than s2.
 *         - Zero if s1 is equal to s2.
 *         - Greater than zero if s1 is greater than s2.
 */
int strcmp_case_insensitive(char *s1, char *s2) {
  register char *ptr1, *ptr2;

  ptr1 = s1;
  ptr2 = s2;
  while (*ptr1 && *ptr2 && tolower(*ptr1) == tolower(*ptr2)) {
    ptr1++;
    ptr2++;
  }
  return ((int)(*ptr1 - *ptr2));
}

/**
 * @brief Compares up to a specified number of characters of two strings in a case-insensitive manner.
 *
 * This function compares the first n characters of two null-terminated strings without considering
 * the case of the characters. It returns an integer less than, equal to, or greater than zero
 * if the first n characters of the first string are found, respectively, to be less than, to match,
 * or be greater than the second string.
 *
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 * @param n The maximum number of characters to compare.
 *
 * @return An integer indicating the relationship between the strings up to n characters:
 *         - Less than zero if s1 is less than s2.
 *         - Zero if the first n characters of s1 are equal to s2.
 *         - Greater than zero if s1 is greater than s2.
 */
int strncmp_case_insensitive(char *s1, char *s2, long n) {
  register char *ptr1, *ptr2;
  register long i;

  ptr1 = s1;
  ptr2 = s2;
  i = 0;
  while (i < n && *ptr1 && *ptr2 && tolower(*ptr1) == tolower(*ptr2)) {
    ptr1++;
    ptr2++;
    i++;
  }

  if (i == n)
    return (0);

  return ((int)(*ptr1 - *ptr2));
}
