/**
 * @file backspace.c
 * @brief Provides functionality to perform backspace operations by a specified number of characters.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author C. Saunders, R. Soliday
 */
#include "mdb.h"

/**
 * @brief Backspace by a specified number of characters.
 *
 * This function outputs a specified number of backspace characters (`\b`) to the standard output.
 * It dynamically allocates and reuses a buffer to store the backspace characters, optimizing
 * memory usage for repeated calls with varying numbers of backspaces.
 *
 * @param n The number of characters to backspace.
 */
void backspace(long n) {
  static char *bspace = NULL;
  static long n_bspace = 0;

  if (n > n_bspace) {
    register long i;
    bspace = trealloc(bspace, sizeof(*bspace) * (n + 1));
    for (i = n_bspace; i < n; i++)
      bspace[i] = '\b';
    n_bspace = n;
  }
  bspace[n] = 0;
  fputs(bspace, stdout);
  bspace[n] = '\b';
}
