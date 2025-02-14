/**
 * @file factorize.c
 * @brief Routines for prime factorization and primality testing.
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
 * @brief Determine if a number is prime.
 *
 * Checks if the given number is prime. 
 * @param number The number to test.
 * @return 1 if the number is prime. Otherwise, returns a negative value which is the negative 
 *         of the smallest prime factor of the number.
 */
int64_t is_prime(int64_t number)
{
  int64_t i, n;

  n = sqrt(number * 1.0) + 1;
  if (n * n > number)
    n--;
  for (i = 2; i <= n; i++)
    if (number % i == 0)
      return (-i);
  return (1);
}

/**
 * @brief Find the smallest prime factor of a number.
 *
 * Returns the smallest prime factor of a given number. If the number is prime, returns the number itself.
 * @param number The number for which to find the smallest factor.
 * @return The smallest factor if found, or the number itself if it is prime. If the number is 1, returns 1.
 */
int64_t smallest_factor(int64_t number)
{
  int64_t i;

  if (number == 1)
    return (1);
  if ((i = is_prime(number)) == 1)
    return (number);
  return (-i);
}

/**
 * @brief Extract the next prime factor from a number.
 *
 * Removes and returns the next prime factor from the given number, dividing it out of the number.
 * Subsequent calls will continue to factorize the updated number.
 * @param number Pointer to the number to factorize. On return, this value is reduced by dividing out the factor.
 * @return The next prime factor if any, otherwise 1 if no further factorization is possible.
 */
int64_t next_prime_factor(int64_t *number) {
  int64_t factor;
  if ((factor = smallest_factor(*number)) > 1) {
    *number /= factor;
    while (*number % factor == 0)
      *number /= factor;
    return factor;
  }
  return 1;
}

/**
 * @brief Find the largest prime factor of a number.
 *
 * Iteratively factorizes the given number to determine its largest prime factor.
 * @param number The number to factorize.
 * @return The largest prime factor of the number, or 1 if the number is 1 or no factorization is possible.
 */
int64_t largest_prime_factor(int64_t number) {
  int64_t factor, lastFactor;
  lastFactor = 1;
  while ((factor = next_prime_factor(&number)) > 1)
    lastFactor = factor;
  return lastFactor;
}
