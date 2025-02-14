/**
 * @file bomb.c
 * @brief Provides functions to report errors and abort the program.
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
 * @brief Reports error messages to the terminal and aborts the program.
 *
 * This function prints the provided error and usage messages to the standard error stream and then terminates the program.
 *
 * @param error A string describing the error.
 * @param usage A string describing the correct usage.
 */
void bomb(char *error, char *usage) {
  if (error)
    fprintf(stderr, "error: %s\n", error);
  if (usage)
    fprintf(stderr, "usage: %s\n", usage);
  exit(1);
}

/**
 * @brief Reports error messages to the terminal and returns a specified value.
 *
 * This function prints the provided error and usage messages to the standard error stream and returns the given return value.
 *
 * @param error A string describing the error.
 * @param usage A string describing the correct usage.
 * @param return_value The value to return after reporting the messages.
 * @return The specified return value.
 */
long bombre(char *error, char *usage, long return_value) {
  if (error)
    fprintf(stderr, "error: %s\n", error);
  if (usage)
    fprintf(stderr, "usage: %s\n", usage);
  return return_value;
}

/**
 * @brief Reports error messages using a printf-style template and aborts the program.
 *
 * This function accepts a format string and a variable number of arguments, formats the message accordingly, prints it to the standard output, and then terminates the program.
 *
 * @param template A printf-style format string.
 * @param ... Variable arguments corresponding to the format string.
 */
void bombVA(char *template, ...) {
  char *p;
  char c, *s;
  int i;
  long j;
  va_list argp;
  double d;

  va_start(argp, template);
  p = template;
  while (*p) {
    if (*p == '%') {
      switch (*++p) {
      case 'l':
        switch (*++p) {
        case 'd':
          j = va_arg(argp, long int);
          printf("%ld", j);
          break;
        case 'e':
          d = va_arg(argp, double);
          printf("%21.15le", d);
          break;
        case 'f':
          d = va_arg(argp, double);
          printf("%lf", d);
          break;
        case 'g':
          d = va_arg(argp, double);
          printf("%21.15lg", d);
          break;
        default:
          printf("%%l%c", *p);
          break;
        }
        break;
      case 'c':
        c = va_arg(argp, int);
        putchar(c);
        break;
      case 'd':
        i = va_arg(argp, int);
        printf("%d", i);
        break;
      case 's':
        s = va_arg(argp, char *);
        fputs(s, stdout);
        break;
      case 'e':
        d = va_arg(argp, double);
        printf("%21.15e", d);
        break;
      case 'f':
        d = va_arg(argp, double);
        printf("%f", d);
        break;
      case 'g':
        d = va_arg(argp, double);
        printf("%21.15g", d);
        break;
      default:
        printf("%%%c", *p);
        break;
      }
    } else {
      putchar(*p);
    }
    p++;
  }
  va_end(argp);
  exit(1);
}
