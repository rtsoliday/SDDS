/**
 * @file binary.c
 * @brief Contains binary conversion functions sbinary and bitsSet.
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

char *sbinary(char *s, int len, long number) {
  long pow_of_2, n, i;
  char c;

  pow_of_2 = 1;
  n = 0;
  strcpy_ss(s, "0");
  while (pow_of_2 > 0 && pow_of_2 <= number && n < len - 1) {
    if (pow_of_2 & number) {
      number -= pow_of_2;
      s[n] = '1';
    } else
      s[n] = '0';
    s[++n] = 0;
    pow_of_2 = pow_of_2 << 1;
  }
  i = -1;
  while (--n > ++i) {
    c = s[n];
    s[n] = s[i];
    s[i] = c;
  }
  return (s);
}

/**
 * @brief Counts the number of set bits (1s) in the given data.
 *
 * This function iterates through each bit of the provided unsigned
 * long integer and counts how many bits are set to 1.
 *
 * @param data The unsigned long integer to count set bits in.
 * @return The number of set bits.
 */
long bitsSet(unsigned long data) {
  long i = 0;
  while (data) {
    if (data & 1)
      i++;
    data >>= 1;
  }
  return i;
}
