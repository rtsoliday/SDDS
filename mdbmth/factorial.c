/**
 * @file factorial.c
 * @brief Provides functions to compute the factorial of a number.
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
 * @brief Computes the factorial of a given number.
 *
 * This function calculates the product of all positive integers up to `n`.
 *
 * @param n The number for which the factorial is to be computed.
 * @return The factorial of `n` as a `long` integer.
 */
long factorial(long n) {
  register long prod = 1;

  while (n > 0)
    prod *= n--;

  return (prod);
}

/**
 * @brief Computes the factorial of a given number as a double.
 *
 * This function calculates the product of all positive integers up to `n` and returns the result as a `double`.
 *
 * @param n The number for which the factorial is to be computed.
 * @return The factorial of `n` as a `double`.
 */
double dfactorial(long n) {
  register double prod = 1;

  while (n > 0)
    prod *= n--;

  return (prod);
}
