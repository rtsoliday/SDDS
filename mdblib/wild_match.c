/**
 * @file wild_match.c
 * @brief Wildcard matching and string utility functions.
 *
 * This file provides functions to perform wildcard pattern matching, expand range
 * specifiers within templates, check for the presence of wildcard characters, and
 * manipulate strings with respect to wildcard handling.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C Saunders, R. Soliday, H. Shang
 */

#if defined(_WIN32)
#  include <stdlib.h>
#endif
#include <ctype.h>
#include "mdb.h"
#define MATCH_INVERT '^'
#define MATCH_MANY '*'
#define MATCH_SET1 '['
#define MATCH_SET2 ']'
#define ESCAPE_CHAR '\\'
#define SET_MATCH_INVERT '^'

#ifdef VAX_VMS
#  define MATCH_ONE '%'
#else
#  define MATCH_ONE '?'
#endif

/**
 * @brief Determine whether one string is a wildcard match for another.
 *
 * Compares the input string against a template that may include wildcard characters
 * such as '*', '?', and character sets defined within brackets. Supports inversion
 * of match results using the '^' character.
 *
 * @param string The string to be matched.
 * @param template The wildcard pattern to match against.
 * @return Returns 1 if the string matches the template, 0 otherwise.
 */
int wild_match(char *string, char *template) {
  char *s, *t, *ptr;
  int len, at_least, invert_set_match, invert_match;

#ifdef DEBUG
  printf("wild_match(%s, %s)\n", string, template);
#endif

  s = string; /* string to check for match */
  len = strlen(s);
  t = template; /* template string, with possible wildcards */
  if (*t == MATCH_INVERT) {
    invert_match = -1;
    t++;
  } else
    invert_match = 0;

  if (!*s) {
    if (!*t)
      return (!invert_match);
    return invert_match;
  } else if (!*t) {
    return invert_match;
  }

  do {
#ifdef DEBUG
    printf("s = %s,  t = %s\n", s, t);
#endif
    switch (*t) {
    case ESCAPE_CHAR:
      t++;
      if (*t++ != *s++)
        return (invert_match);
      len--;
      break;
    case MATCH_MANY:
      at_least = 0;
      while (*t) {
        if (*t == MATCH_MANY)
          t++;
        else if (*t == MATCH_ONE) {
          at_least++;
          t++;
        } else
          break;
      }
      if (at_least > len)
        return (invert_match);
      s += at_least;
      if (*t == 0 && *(t - 1) == MATCH_MANY) {
#ifdef DEBUG
        printf("return(1)\n");
#endif
        return (!invert_match);
      }
      ptr = s;
      while (*ptr) {
        if (wild_match(ptr, t)) {
#ifdef DEBUG
          printf("return(2)\n");
#endif
          return (!invert_match);
        }
        ptr++;
      }
      ptr = s;
      while ((ptr = strchr(ptr, *t))) {
        if (wild_match(ptr + 1, t + 1)) {
#ifdef DEBUG
          printf("return(3)\n");
#endif
          return (!invert_match);
        }
        if (*++ptr)
          ++ptr;
      }
      return (invert_match);
    case MATCH_ONE:
      s++;
      t++;
      len--;
      break;
    case MATCH_SET1:
      ptr = NULL;
      if (!*(t + 1) || !(ptr = strchr(t + 1, MATCH_SET2))) {
        if (*++t != *++s)
          return (invert_match);
        len--;
      } else {
        *ptr++ = 0;
        t++;
        SWAP_PTR(ptr, t);
        invert_set_match = 0;
        if (*ptr == SET_MATCH_INVERT && strlen(ptr) != 1) {
          invert_set_match = 1;
          ptr++;
        }
        if (strchr(ptr, *s)) {
          *(t - 1) = MATCH_SET2;
          if (invert_set_match)
            return (invert_match);
          s++;
          len--;
        } else {
          *(t - 1) = MATCH_SET2;
          if (!invert_set_match)
            return (invert_match);
          s++;
          len--;
        }
      }
      break;
    default:
      if (*s++ != *t++)
        return (invert_match);
      len--;
      break;
    }
  } while (*s && *t);
#ifdef DEBUG
  printf("s = %s,  t = %s\n", s, t);
#endif
  if (!*s && !*t) {
#ifdef DEBUG
    printf("return(5)\n");
#endif
    return (!invert_match);
  }
  if (!*s && *t == MATCH_MANY && !*(t + 1)) {
#ifdef DEBUG
    printf("return(4)\n");
#endif
    return (!invert_match);
  }
  if (*s && !*t) {
#ifdef DEBUG
    printf("return(6)\n");
#endif
    return (invert_match);
  }
  if (!*s && *t) {
    while (*t) {
      if (*t != MATCH_MANY) {
#ifdef DEBUG
        printf("return(7)\n");
#endif
        return (invert_match);
      }
      t++;
    }
#ifdef DEBUG
    printf("return(8)\n");
#endif
    return !invert_match;
  }
  bomb("the impossible has happened (wild_match)", NULL);
  exit(1);
}

/**
 * @brief Determine whether one string is a case-insensitive wildcard match for another.
 *
 * Similar to `wild_match`, but performs case-insensitive comparisons between the
 * input string and the template. Handles wildcard characters and supports inversion
 * of match results.
 *
 * @param string The string to be matched, case-insensitively.
 * @param template The wildcard pattern to match against, case-insensitively.
 * @return Returns 1 if the string matches the template, 0 otherwise.
 */
int wild_match_ci(char *string, char *template) {
  char *s, *t, *ptr;
  int len, at_least, invert_set_match, invert_match;

  s = string; /* string to check for match */
  len = strlen(s);
  t = template; /* template string, with possible wildcards */
  if (*t == MATCH_INVERT) {
    invert_match = -1;
    t++;
  } else
    invert_match = 0;

  if (!*s) {
    if (!*t)
      return (!invert_match);
    return invert_match;
  } else if (!*t) {
    return invert_match;
  }

  do {
#ifdef DEBUG
    printf("s = %s,  t = %s\n", s, t);
#endif
    switch (*t) {
    case ESCAPE_CHAR:
      t++;
      if (tolower(*t) != tolower(*s))
        return (invert_match);
      t++;
      s++;
      len--;
      break;
    case MATCH_MANY:
      at_least = 0;
      while (*t) {
        if (*t == MATCH_MANY)
          t++;
        else if (*t == MATCH_ONE) {
          at_least++;
          t++;
        } else
          break;
      }
      if (at_least > len)
        return (invert_match);
      s += at_least;
      if (*t == 0 && *(t - 1) == MATCH_MANY) {
#ifdef DEBUG
        printf("return(1)\n");
#endif
        return (!invert_match);
      }
      ptr = s;
      while (*ptr) {
        if (wild_match_ci(ptr, t)) {
#ifdef DEBUG
          printf("return(2)\n");
#endif
          return (!invert_match);
        }
        ptr++;
      }
      ptr = s;
      while ((ptr = strchr_ci(ptr, *t))) {
        if (wild_match_ci(ptr + 1, t + 1)) {
#ifdef DEBUG
          printf("return(3)\n");
#endif
          return (!invert_match);
        }
        if (*++ptr)
          ++ptr;
      }
      return (invert_match);
    case MATCH_ONE:
      s++;
      t++;
      len--;
      break;
    case MATCH_SET1:
      ptr = NULL;
      if (!*(t + 1) || !(ptr = strchr_ci(t + 1, MATCH_SET2))) {
        if (tolower(*t) != tolower(*s))
          return (invert_match);
        t++;
        s++;
        len--;
      } else {
        *ptr++ = 0;
        t++;
        SWAP_PTR(ptr, t);
        invert_set_match = 0;
        if (*ptr == SET_MATCH_INVERT && strlen(ptr) != 1) {
          invert_set_match = 1;
          ptr++;
        }
        if (strchr_ci(ptr, *s)) {
          *(t - 1) = MATCH_SET2;
          if (invert_set_match)
            return (invert_match);
          s++;
          len--;
        } else {
          *(t - 1) = MATCH_SET2;
          if (!invert_set_match)
            return (invert_match);
          s++;
          len--;
        }
      }
      break;
    default:
      if (tolower(*s) != tolower(*t))
        return (invert_match);
      t++;
      s++;
      len--;
      break;
    }
  } while (*s && *t);
#ifdef DEBUG
  printf("s = %s,  t = %s\n", s, t);
#endif
  if (!*s && !*t) {
#ifdef DEBUG
    printf("return(5)\n");
#endif
    return (!invert_match);
  }
  if (!*s && *t == MATCH_MANY && !*(t + 1)) {
#ifdef DEBUG
    printf("return(4)\n");
#endif
    return (!invert_match);
  }
  if (*s && !*t) {
#ifdef DEBUG
    printf("return(6)\n");
#endif
    return (invert_match);
  }
  if (!*s && *t) {
    while (*t) {
      if (*t != MATCH_MANY) {
#ifdef DEBUG
        printf("return(7)\n");
#endif
        return (invert_match);
      }
      t++;
    }
#ifdef DEBUG
    printf("return(8)\n");
#endif
    return !invert_match;
  }
  bomb("the impossible has happened (wild_match_ci)", NULL);
  exit(1);
}

/**
 * @brief Compare two strings case-insensitively.
 *
 * Performs a case-insensitive comparison between two strings. Returns 0 if the strings
 * are equal, a negative value if the first non-matching character in `s1` is lower
 * than that in `s2`, and a positive value if it is greater.
 *
 * @param s The first string to compare.
 * @param t The second string to compare.
 * @return An integer indicating the relationship between the strings:
 *         -1 if `s1` < `s2`,
 *          0 if `s1` == `s2`,
 *          1 if `s1` > `s2`.
 */
int strcmp_ci(const char *s, const char *t) {
  char sc, tc;
  while (*s && *t) {
    if ((sc = tolower(*s)) < (tc = tolower(*t)))
      return -1;
    if (sc > tc)
      return 1;
    s++;
    t++;
  }
  return 0;
}

char *strchr_ci(char *s, char c) {
  c = tolower(c);
  while (*s) {
    if (tolower(*s) == c)
      return s;
    s++;
  }
  return NULL;
}

/**
 * @brief Expand range specifiers in a wildcard template into explicit character lists.
 *
 * Processes a wildcard template containing range specifiers (e.g., `[a-e0-5]`) and
 * expands them into explicit lists of characters (e.g., `[abcde012345]`). This
 * function should be called before performing wildcard matching with `wild_match()`.
 *
 * @param template The wildcard template containing range specifiers.
 * @return A newly allocated string with all range specifiers expanded into explicit lists.
 */
char *expand_ranges(char *template) {
  char *new, *ptr, *ptr1, *ptr2, *end_new;
  int n_in_range;

  end_new = new = tmalloc(sizeof(*new) * (strlen(template) + 1));
  *new = 0;
  ptr = tmalloc(sizeof(*new) * (strlen(template) + 1));
  strcpy(ptr, template);
  while (*ptr) {
    if (*ptr == ESCAPE_CHAR) {
      *end_new++ = *ptr++;
      if (*ptr)
        *end_new++ = *ptr++;
      *end_new = 0;
    } else if (*ptr == MATCH_SET1) {
      *end_new++ = *ptr++;
      if ((ptr1 = strchr(ptr, MATCH_SET2))) {
        *ptr1 = 0;
        ptr2 = ptr;
        while (*ptr2) {
          *end_new++ = *ptr2++;
          *end_new = 0;
          if (*ptr2 == '-') {
            if (*(ptr2 - 1) == ESCAPE_CHAR) {
              *(end_new - 1) = '-';
              *end_new = 0;
              ptr2++;
            } else {
              ptr2++;
              n_in_range = (*ptr2) - (*(ptr2 - 2));
              if (n_in_range <= 0) {
                fprintf(stderr, "error: bad range syntax: %s\n", ptr - 2);
                exit(1);
              }
              new = trealloc(new, sizeof(*new) * (strlen(new) + n_in_range + strlen(ptr1 + 1) + 2));
              end_new = new + strlen(new);
              while (n_in_range--)
                *end_new++ = *ptr2 - n_in_range;
              *end_new = 0;
              ptr2++;
            }
          }
        }
        *end_new++ = *ptr1 = MATCH_SET2;
        *end_new = 0;
        ptr = ptr1 + 1;
      } else {
        *end_new++ = *(ptr - 1);
        *end_new = 0;
      }
    } else {
      *end_new++ = *ptr++;
      *end_new = 0;
    }
  }
  *end_new = 0;
  return (new);
}

/**
 * @brief Check if a template string contains any wildcard characters.
 *
 * Scans the input template string to determine if it includes any wildcard characters
 * such as '*', '?', or character sets defined within brackets. Escaped wildcard
 * characters (preceded by a backslash) are ignored.
 *
 * @param template The string to check for wildcard characters.
 * @return Returns 1 if any unescaped wildcard characters are found, 0 otherwise.
 */
int has_wildcards(char *template) {
  char *ptr;

  ptr = template;
  while ((ptr = strchr(ptr, MATCH_MANY))) {
    if (ptr == template || *(ptr - 1) != ESCAPE_CHAR)
      return (1);
    ptr++;
  }

  ptr = template;
  while ((ptr = strchr(ptr, MATCH_ONE))) {
    if (ptr == template || *(ptr - 1) != ESCAPE_CHAR)
      return (1);
    ptr++;
  }

  ptr = template;
  while ((ptr = strchr(ptr, MATCH_SET1))) {
    if (ptr == template || *(ptr - 1) != ESCAPE_CHAR)
      return (1);
    ptr++;
  }

  return (0);
}

/**
 * @brief Remove escape characters from wildcard characters in a template string.
 *
 * Processes the input template string and removes backslashes preceding wildcard
 * characters ('*', '?', '[', ']'). This function modifies the string in place.
 *
 * @param template The wildcard template string to unescape.
 * @return A pointer to the modified template string with escape characters removed.
 */
char *unescape_wildcards(char *template) {
  char *ptr;

  ptr = template;
  while ((ptr = strchr(ptr, MATCH_MANY))) {
    if (ptr != template && *(ptr - 1) == ESCAPE_CHAR) {
      strcpy_ss(ptr - 1, ptr);
    }
    ptr++;
  }
  ptr = template;
  while ((ptr = strchr(ptr, MATCH_ONE))) {
    if (ptr != template && *(ptr - 1) == ESCAPE_CHAR) {
      strcpy_ss(ptr - 1, ptr);
    }
    ptr++;
  }
  ptr = template;
  while ((ptr = strchr(ptr, MATCH_SET1))) {
    if (ptr != template && *(ptr - 1) == ESCAPE_CHAR) {
      strcpy_ss(ptr - 1, ptr);
    }
    ptr++;
  }
  ptr = template;
  while ((ptr = strchr(ptr, MATCH_SET2))) {
    if (ptr != template && *(ptr - 1) == ESCAPE_CHAR) {
      strcpy_ss(ptr - 1, ptr);
    }
    ptr++;
  }
  return (template);
}

/**
 * @brief Compare two strings with a custom non-hierarchical ranking.
 *
 * Compares two strings where non-numeric characters are ranked before numeric characters.
 * Numeric characters are compared based on their length, with single-digit numbers
 * ranked before multi-digit numbers. The comparison returns -1, 0, or 1 depending
 * on whether the first string is less than, equal to, or greater than the second string.
 *
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 * @return An integer indicating the relationship between the strings:
 *         -1 if `s1` < `s2`,
 *          0 if `s1` == `s2`,
 *          1 if `s1` > `s2`.
 */
int strcmp_nh(const char *s1, const char *s2) {
  int n1, n2, n3, n4;
  int i;
  for (; *s1 && *s2; s1++, s2++) {
    if (((*s1 >= '0') && (*s1 <= '9')))
      n1 = 1;
    else
      n1 = 0;
    if (((*s2 >= '0') && (*s2 <= '9')))
      n2 = 1;
    else
      n2 = 0;
    if ((n1 == 1) && (n2 == 1)) {
      i = 1;
      while ((*(s1 + i)) && (*(s2 + i))) {
        if (((*(s1 + i) >= '0') && (*(s1 + i) <= '9')))
          n3 = 1;
        else
          n3 = 0;
        if (((*(s2 + i) >= '0') && (*(s2 + i) <= '9')))
          n4 = 1;
        else
          n4 = 0;
        if ((n3 == 1) && (n4 == 0))
          return (1);
        else if ((n3 == 0) && (n4 == 1))
          return (-1);
        else if ((n3 == 0) && (n4 == 0))
          break;
        i++;
      }
      if (((*(s1 + i)) == 0) && (*(s2 + i))) {
        if (((*(s2 + i) >= '0') && (*(s2 + i) <= '9')))
          return (-1);
      } else if (((*(s2 + i)) == 0) && (*(s1 + i))) {
        if (((*(s1 + i) >= '0') && (*(s1 + i) <= '9')))
          return (1);
      }
    }

    if (n1 == n2) {
      if (*s1 < *s2)
        return (-1);
      else if (*s1 > *s2)
        return (1);
    } else if (n1 == 0) {
      return (-1);
    } else {
      return (1);
    }
  }
  if (*s1)
    return (1);
  else if (*s2)
    return (-1);
  return (0);
}
