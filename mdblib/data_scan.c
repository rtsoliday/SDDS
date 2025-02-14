/**
 * @file data_scan.c
 * @brief Provides functions for scanning and parsing free-format data.
 *
 * This file contains functions such as get_double(), get_float(), get_long(),
 * get_token(), and get_token_buf() to facilitate easy scanning and parsing of
 * numerical and token-based data from strings.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include <ctype.h>

#define float_start(m_c) (isdigit(*(m_c)) || *(m_c) == '.' || ((*(m_c) == '-' || *(m_c) == '+') && (isdigit(*((m_c) + 1)) || *((m_c) + 1) == '.')))
#define int_start(m_c) (isdigit(*(m_c)) || ((*(m_c) == '-' || *(m_c) == '+') && isdigit(*((m_c) + 1))))

#define skip_it(m_c) (isspace(m_c) || (m_c) == ',' || (m_c) == ';')
#define nskip_it(m_c) (!isspace(m_c) && (m_c) != ',' && (m_c) != ';')

/**
 * @brief Parses a double value from the given string.
 *
 * This function scans the input string for a valid double-precision floating-point
 * number, stores the result in the provided pointer, and updates the string pointer
 * to the position following the parsed number.
 *
 * @param[out] dptr Pointer to store the parsed double value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if a double was successfully parsed, 0 otherwise.
 */
int get_double(double *dptr, char *s) {
  register char *ptr0;
  register int was_point = 0;

  /* skip leading non-number characters */
  ptr0 = s;
  while (!float_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%lf", dptr);

  /* skip to first white-space following number */

  /* skip sign, if any */
  if (*s == '-' || *s == '+')
    s++;

  /* skip mantissa */
  while (isdigit(*s) || (*s == '.' && !was_point))
    if (*s++ == '.')
      was_point = 1;

  if (*s == 'e' || *s == 'E') { /* skip exponent, if any */
    s++;
    if (*s == '+' || *s == '-')
      s++;
    while (isdigit(*s))
      s++;
  }

  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Parses a long double value from the given string.
 *
 * This function scans the input string for a valid long double floating-point number,
 * stores the result in the provided pointer, and updates the string pointer to the
 * position following the parsed number.
 *
 * @param[out] dptr Pointer to store the parsed long double value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if a long double was successfully parsed, 0 otherwise.
 */
int get_longdouble(long double *dptr, char *s) {
  register char *ptr0;
  register int was_point = 0;

  /* skip leading non-number characters */
  ptr0 = s;
  while (!float_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);
  /* scan the number */
  sscanf(s, "%Lf", dptr);
  /* skip to first white-space following number */

  /* skip sign, if any */
  if (*s == '-' || *s == '+')
    s++;

  /* skip mantissa */
  while (isdigit(*s) || (*s == '.' && !was_point))
    if (*s++ == '.')
      was_point = 1;

  if (*s == 'e' || *s == 'E') { /* skip exponent, if any */
    s++;
    if (*s == '+' || *s == '-')
      s++;
    while (isdigit(*s))
      s++;
  }
  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Parses a double value from the given string without modifying the string.
 *
 * This function scans the input string for a valid double-precision floating-point
 * number, stores the result in the provided pointer, and ensures that the string remains
 * unchanged after parsing.
 *
 * @param[out] dptr Pointer to store the parsed double value.
 * @param[in] s Input string to scan. It remains unchanged after parsing.
 * @return Returns 1 if a double was successfully parsed, 0 otherwise.
 */
int get_double1(double *dptr, char *s) {
    char *endptr;
    double val;

    if (s == NULL || *s == '\0')
        return 0;

    // Remove trailing whitespace
    char *p = s + strlen(s) - 1;
    while (p >= s && isspace((unsigned char)*p))
        p--;
    *(p + 1) = '\0';

    // Start from the beginning and try to find a number that ends at the end of the string
    char *start = s;
    while (*start) {
        val = strtod(start, &endptr);

        if (endptr != start) {
            // Check if we've reached the end of the string
            if (*endptr == '\0') {
                *dptr = val;
                return 1;
            }
        }
        start++;
    }
    return 0;
}

int get_double1_old(double *dptr, char *s) {
  register int was_point = 0;

  /* skip leading non-number characters */
  while (!float_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%lf", dptr);

  /* skip to first white-space following number */

  /* skip sign, if any */
  if (*s == '-' || *s == '+')
    s++;

  /* skip mantissa */
  while (isdigit(*s) || (*s == '.' && !was_point))
    if (*s++ == '.')
      was_point = 1;

  if (*s == 'e' || *s == 'E') { /* skip exponent, if any */
    s++;
    if (*s == '+' || *s == '-')
      s++;
    while (isdigit(*s))
      s++;
  }

  return (1);
}

/**
 * @brief Parses a float value from the given string.
 *
 * This function scans the input string for a valid single-precision floating-point
 * number, stores the result in the provided pointer, and updates the string pointer to the
 * position following the parsed number.
 *
 * @param[out] fptr Pointer to store the parsed float value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if a float was successfully parsed, 0 otherwise.
 */
int get_float(float *fptr, char *s) {
  register char *ptr0;
  register int was_point = 0;

  /* skip leading non-number characters */
  ptr0 = s;
  while (!float_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%f", fptr);

  /* skip to first white-space following number */

  /* skip sign, if any */
  if (*s == '-' || *s == '+')
    s++;

  /* skip mantissa */
  while (isdigit(*s) || (*s == '.' && !was_point))
    if (*s++ == '.')
      was_point = 1;

  if (*s == 'e' || *s == 'E') { /* skip exponent, if any */
    s++;
    if (*s == '+' || *s == '-')
      s++;
    while (isdigit(*s))
      s++;
  }

  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Parses a long integer value from the given string.
 *
 * This function scans the input string for a valid long integer, stores the result in the
 * provided pointer, and updates the string pointer to the position following the parsed number.
 *
 * @param[out] iptr Pointer to store the parsed long integer value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if a long integer was successfully parsed, 0 otherwise.
 */
int get_long(long *iptr, char *s) {
  char *ptr0;

  /* skip leading white-space and commas */
  ptr0 = s;
  while (!int_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%ld", iptr);

  /* skip to first white-space following number */
  if (*s == '-' || *s == '+')
    s++;
  while (isdigit(*s))
    s++;

  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Parses a long integer value from the given string without modifying the string.
 *
 * This function scans the input string for a valid long integer, stores the result in the
 * provided pointer, and ensures that the string remains unchanged after parsing.
 *
 * @param[out] lptr Pointer to store the parsed long integer value.
 * @param[in] s Input string to scan. It remains unchanged after parsing.
 * @return Returns 1 if a long integer was successfully parsed, 0 otherwise.
 */
int get_long1(long *lptr, char *s) {
    char *endptr;
    long val;

    if (s == NULL || *s == '\0')
        return 0;

    // Remove trailing whitespace
    char *p = s + strlen(s) - 1;
    while (p >= s && isspace((unsigned char)*p))
        p--;
    *(p + 1) = '\0';

    // Start from the beginning and try to find a number that ends at the end of the string
    char *start = s;
    while (*start) {
        val = strtol(start, &endptr, 10);  // Base 10

        if (endptr != start) {
            // Check if we've reached the end of the string
            if (*endptr == '\0') {
                *lptr = val;
                return 1;
            }
        }
        start++;
    }
    return 0;
}

int get_long1_old(long *iptr, char *s) {

  /* skip leading white-space and commas */
  while (!int_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%ld", iptr);

  /* skip to first white-space following number */
  if (*s == '-' || *s == '+')
    s++;
  while (isdigit(*s))
    s++;
  return (1);
}

/**
 * @brief Parses a short integer value from the given string.
 *
 * This function scans the input string for a valid short integer, stores the result in the
 * provided pointer, and updates the string pointer to the position following the parsed number.
 *
 * @param[out] iptr Pointer to store the parsed short integer value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if a short integer was successfully parsed, 0 otherwise.
 */
int get_short(short *iptr, char *s) {
  char *ptr0;

  /* skip leading white-space and commas */
  ptr0 = s;
  while (!int_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%hd", iptr);

  /* skip to first white-space following number */
  if (*s == '-' || *s == '+')
    s++;
  while (isdigit(*s))
    s++;

  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Parses an integer value from the given string.
 *
 * This function scans the input string for a valid integer, stores the result in the
 * provided pointer, and updates the string pointer to the position following the parsed number.
 *
 * @param[out] iptr Pointer to store the parsed integer value.
 * @param[in,out] s Input string to scan. It will be modified to point after the parsed number.
 * @return Returns 1 if an integer was successfully parsed, 0 otherwise.
 */
int get_int(int *iptr, char *s) {
  char *ptr0;

  /* skip leading white-space and commas */
  ptr0 = s;
  while (!int_start(s) && *s)
    s++;
  if (*s == 0)
    return (0);

  /* scan the number */
  sscanf(s, "%d", iptr);

  /* skip to first white-space following number */
  if (*s == '-' || *s == '+')
    s++;
  while (isdigit(*s))
    s++;

  strcpy_ss(ptr0, s);
  return (1);
}

/**
 * @brief Extracts the next token from the input string.
 *
 * This function scans the input string for the next token, which can be a quoted string
 * or a sequence of non-separator characters. It allocates memory for the token, copies it
 * to the allocated space, and updates the input string pointer to the position following the token.
 *
 * @param[in,out] s Input string to scan. It will be modified to point after the extracted token.
 * @return Returns a pointer to the newly allocated token string, or NULL if no token is found.
 */
char *get_token(char *s) {
  char *ptr0, *ptr1, *ptr;

  /* skip leading white-space and commas */
  ptr0 = s;
  while (skip_it(*s))
    s++;
  if (*s == 0)
    return (NULL);
  ptr1 = s;

  if (*s == '"' && (ptr0 == s || *(s - 1) != '\\')) {
    ptr1 = s + 1;
    /* quoted string, so skip to next quotation mark */
    do {
      s++;
    } while (*s && (*s != '"' || *(s - 1) == '\\'));
    if (*s == '"' && *(s - 1) != '\\')
      *s = ' ';
  } else {
    /* skip to first white-space following token */
    do {
      s++;
      if (*s == '"' && *(s - 1) != '\\') {
        while (*++s && (*s != '"' || *(s - 1) == '\\'))
          ;
      }
    } while (*s && nskip_it(*s));
  }

  ptr = tmalloc((unsigned)sizeof(*ptr) * (s - ptr1 + 1));
  strncpy(ptr, ptr1, s - ptr1);
  ptr[s - ptr1] = 0;

  strcpy_ss(ptr0, s);
  return (ptr);
}

/**
 * @brief Extracts the next token from the input string into a provided buffer.
 *
 * This function scans the input string for the next token, which can be a quoted string
 * or a sequence of non-separator characters. It copies the token into the provided buffer
 * if it fits, and updates the input string pointer to the position following the token.
 *
 * @param[in,out] s Input string to scan. It will be modified to point after the extracted token.
 * @param[out] buf Buffer to store the extracted token.
 * @param[in] lbuf Length of the buffer to prevent overflow.
 * @return Returns a pointer to the buffer containing the token, or NULL if no token is found.
 */
char *get_token_buf(char *s, char *buf, long lbuf) {
  char *ptr0, *ptr1;

  /* skip leading white-space and commas */
  ptr0 = s;
  while (skip_it(*s))
    s++;
  if (*s == 0)
    return (NULL);
  ptr1 = s;

  if (*s == '"') {
    ptr1 = s + 1;
    /* if quoted string, skip to next quotation mark */
    do {
      s++;
    } while (*s != '"' && *s);
    if (*s == '"')
      *s = ' ';
  } else {
    /* skip to first white-space following token */
    do {
      s++;
    } while (*s && nskip_it(*s));
  }

  if ((s - ptr1 + 1) > lbuf) {
    printf("buffer overflow in get_token_buf()\nstring was %s\n", ptr0);
    exit(1);
  }
  strncpy(buf, ptr1, s - ptr1);
  buf[s - ptr1] = 0;

  strcpy_ss(ptr0, s);
  return (buf);
}

/**
 * @brief Checks if the given token represents a valid integer.
 *
 * This function verifies whether the input token string is a valid integer, which may
 * include an optional leading '+' or '-' sign followed by digits.
 *
 * @param[in] token The token string to check.
 * @return Returns a non-zero value if the token is a valid integer, 0 otherwise.
 */
long tokenIsInteger(char *token) {
  if (!isdigit(*token) && *token != '-' && *token != '+')
    return 0;
  if (!isdigit(*token) && !isdigit(*(token + 1)))
    return 0;
  token++;
  while (*token)
    if (!isdigit(*token++))
      return 0;
  return 1;
}

/**
 * @brief Checks if the given token represents a valid number.
 *
 * This function verifies whether the input token string is a valid numerical value,
 * which may include integers, floating-point numbers, and numbers in exponential notation.
 *
 * @param[in] token The token string to check.
 * @return Returns a non-zero value if the token is a valid number, 0 otherwise.
 */
long tokenIsNumber(char *token) {
  long pointSeen, digitSeen;

  if (!(digitSeen = isdigit(*token)) && *token != '-' && *token != '+' && *token != '.')
    return 0;
  pointSeen = *token == '.';
  token++;

  while (*token) {
    if (isdigit(*token)) {
      digitSeen = 1;
      token++;
    } else if (*token == '.') {
      if (pointSeen)
        return 0;
      pointSeen = 1;
      token++;
    } else if (*token == 'e' || *token == 'E') {
      if (!digitSeen)
        return 0;
      return tokenIsInteger(token + 1);
    } else
      return 0;
  }
  return digitSeen;
}
