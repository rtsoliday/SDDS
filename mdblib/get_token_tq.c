/**
 * @file get_token_tq.c
 * @brief Functions for parsing tokens from strings with support for delimiters and quotations.
 *
 * This file provides implementations for extracting tokens from character strings
 * based on specified delimiter sets and quotation marks. It includes functions to
 * handle nested quotations and escaped characters, facilitating robust tokenization
 * of complex strings.
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

char *seek_level(char *s, char qs, char qe);

int in_charset(char c, char *s);

/**
 * @brief Extracts a token from a string based on delimiter characters.
 *
 * The `get_token_t` function retrieves a token from the input string `s`, where a token
 * is defined as a sequence of characters bounded by any of the delimiter characters
 * specified in the string `t`. It intelligently handles quoted sections, allowing delimiters
 * within quotes to be part of the token.
 *
 * @param s The input string to parse.
 * @param t A string containing delimiter characters.
 * @return A dynamically allocated string containing the extracted token, or NULL if no token is found.
 */
char *get_token_t(char *s, char *t) {
  char *ptr0, *ptr1, *ptr;

  /* skip all leading characters of s found in string t */
  ptr0 = s;
  while (in_charset(*s, t) && *s)
    s++;
  if (*s == 0)
    return (NULL);
  ptr1 = s;

  /* skip to next character of s found in t, skipping over quoted */
  /* portions */
  do {
    if (*s == '"' && !(s != ptr0 && *(s - 1) == '\\')) {
      s++;
      while (*s && !(*s == '"' && *(s - 1) != '\\'))
        s++;
      if (*s == '"')
        s++;
    } else
      s++;
  } while (*s && !in_charset(*s, t));

  ptr = tmalloc(sizeof(*ptr) * (s - ptr1 + 1));
  strncpy(ptr, ptr1, s - ptr1);
  ptr[s - ptr1] = 0;

  strcpy_ss(ptr0, s);

  interpret_escaped_quotes(ptr);
  return (ptr);
}

/* routine: in_charset()
 * purpose: determine if character is a member of a set of characters.
 *          Returns the number of the character that matches, or zero
 *          for no match.
 */

int in_charset(char c, char *set) {
  register int i;

  i = 1;
  while (*set) {
    if (*set == c)
      return (i);
    set++;
    i++;
  }
  return (0);
}

/**
 * @brief Extracts a token from a string with support for multiple delimiter and quotation sets.
 *
 * The `get_token_tq` function retrieves a token from the input string `s` based on two sets of
 * delimiter characters (`ts` for token start and `te` for token end) and two sets of quotation
 * characters (`qs` for quotation start and `qe` for quotation end). It ensures that delimiters
 * within quoted sections are ignored, allowing for nested or paired quotations.
 *
 * @param s The input string to parse.
 * @param ts A string containing token start delimiter characters.
 * @param te A string containing token end delimiter characters.
 * @param qs A string containing quotation start characters.
 * @param qe A string containing quotation end characters.
 * @return A dynamically allocated string containing the extracted token, or NULL if no token is found.
 */
char *get_token_tq(s, ts, te, qs, qe) char *s; /* the string to be scanned */
char *ts, *te;                                 /* strings of token start and end characters */
char *qs, *qe;                                 /* strings of starting and ending quotation characters */
{
  register char *ptr0, *ptr1, *ptr;
  register int in_quotes;

  /* skip all leading characters of s found in string t */
  ptr0 = s;
  while (*s && in_charset(*s, ts) && !in_charset(*s, qs)) {
    s++;
  }
  if (*s == 0)
    return (NULL);
  ptr1 = s;
  if ((in_quotes = in_charset(*s, qs)))
    s++;

  /* skip to next character of s found in t */
  do {
    if (in_quotes) {
      if ((ptr = seek_level(s, *(qs + in_quotes - 1), *(qe + in_quotes - 1)))) {
        s = ptr;
        in_quotes = 0;
      } else {
        s += strlen(s);
        in_quotes = 0;
      }
    } else {
      in_quotes = in_charset(*s, qs);
      s++;
    }
  } while (*s && (in_quotes || !in_charset(*s, te)));

  ptr = tmalloc((unsigned)sizeof(*ptr) * (s - ptr1 + 1));
  strncpy(ptr, ptr1, s - ptr1);
  ptr[s - ptr1] = 0;

  if (*s)
    strcpy_ss(ptr0, s + 1);
  else
    *ptr0 = *s;

  interpret_escaped_quotes(ptr);
  return (ptr);
}

/* routine: seek_level()
 * purpose: seek through a string to find the place where a parenthesis
 *          or quotation-mark nesting of zero occurs, assuming that
 *          a nesting of 1 existed prior to the call.  Note that if
 *          qs=qe, the routine simply finds the next quotation-mark.
 *          The routine returns a pointer to the place in the string
 *          where the nesting is zero (after the last quotation-mark
 *          or parenthesis).
 */

char *seek_level(char *s, char qs, char qe) {
  register int qlevel;
  char *ptr0;

  ptr0 = s;
  qlevel = 1;
  while (*s && qlevel) {
    if (*s == qe && !(s != ptr0 && *(s - 1) == '\\'))
      qlevel--;
    else if (*s == qs && !(s != ptr0 && *(s - 1) == '\\'))
      qlevel++;
    s++;
  }
  if (qlevel == 0)
    return (s);
  return (NULL);
}

/**
 * @brief Processes a string to interpret and replace escaped quotation marks.
 *
 * The `interpret_escaped_quotes` function scans the input string `s` and replaces any escaped
 * quotation marks (e.g., `\"`) with actual quotation marks (`"`), effectively removing the escape
 * character and preserving the intended quote in the string.
 *
 * @param s The string in which to interpret escaped quotes.
 */
void interpret_escaped_quotes(char *s) {
  char *ptr;

  ptr = s;
  while (*ptr) {
    if (*ptr == '\\' && *(ptr + 1) == '"')
      strcpy_ss(ptr, ptr + 1);
    else
      ptr++;
  }
}
