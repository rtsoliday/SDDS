/**
 * @file query.c
 * @brief Provides functions to prompt the user for various types of numerical input.
 *
 * This file contains functions that display prompts to the terminal for different
 * numerical types, show default values, and return the user-entered value or the
 * default if no input is provided.
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
 * @brief Prompts the user for a double precision number.
 *
 * Displays a prompt with a default value. If the user does not enter a number,
 * the default value is returned.
 *
 * @param prompt The message to display to the user.
 * @param def The default double value to return if no input is provided.
 * @return The double value entered by the user or the default value.
 */
double query_double(char *prompt, double def) {
  static char s[100];
  static double val;

  printf("%s [%g]: ", prompt, def);
  fgets(s, 99, stdin);
  val = def;
  if (*s)
    sscanf(s, "%lf", &val);
  return (val);
}

/**
 * @brief Prompts the user for a float precision number.
 *
 * Displays a prompt with a default value. If the user does not enter a number,
 * the default value is returned.
 *
 * @param prompt The message to display to the user.
 * @param def The default float value to return if no input is provided.
 * @return The float value entered by the user or the default value.
 */
float query_float(char *prompt, float def) {
  static char s[100];
  static float val;

  printf("%s [%g]: ", prompt, def);
  fgets(s, 99, stdin);
  val = def;
  if (*s)
    sscanf(s, "%f", &val);
  return (val);
}

/**
 * @brief Prompts the user for a long integer.
 *
 * Displays a prompt with a default value. If the user does not enter a number,
 * the default value is returned.
 *
 * @param prompt The message to display to the user.
 * @param def The default long integer value to return if no input is provided.
 * @return The long integer entered by the user or the default value.
 */
long query_long(char *prompt, long def) {
  static char s[100];
  static long val;

  printf("%s [%ld]: ", prompt, def);
  fgets(s, 99, stdin);
  val = def;
  if (*s)
    sscanf(s, "%ld", &val);
  return (val);
}

/**
 * @brief Prompts the user for an integer.
 *
 * Displays a prompt with a default value. If the user does not enter a number,
 * the default value is returned.
 *
 * @param prompt The message to display to the user.
 * @param def The default integer value to return if no input is provided.
 * @return The integer entered by the user or the default value.
 */
int query_int(char *prompt, int def) {
  static char s[100];
  static int val;

  printf("%s [%d]: ", prompt, def);
  fgets(s, 99, stdin);
  val = def;
  if (*s)
    sscanf(s, "%d", &val);
  return (val);
}

/**
 * @brief Prompts the user for a short integer.
 *
 * Displays a prompt with a default value. If the user does not enter a number,
 * the default value is returned.
 *
 * @param prompt The message to display to the user.
 * @param def The default short integer value to return if no input is provided.
 * @return The short integer entered by the user or the default value.
 */
short query_short(char *prompt, short def) {
  static char s[100];
  static short val;

  printf("%s [%d]: ", prompt, def);
  fgets(s, 99, stdin);
  val = def;
  if (*s)
    sscanf(s, "%hd", &val);
  return (val);
}
