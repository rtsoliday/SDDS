/**
 * @file counter.c
 * @brief Provides functions to sequence values over an n-dimensional grid.
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
 * @brief Sequences an array of values systematically to cover an n-dimensional grid.
 *
 * @param value Pointer to the array of values to be updated.
 * @param value_index Pointer to the array of value indices.
 * @param initial Pointer to the array of initial values.
 * @param step Pointer to the array of step sizes.
 * @param n_values Number of values in the array.
 * @param counter Pointer to the counter array.
 * @param max_count Pointer to the maximum count array.
 * @param n_indices Number of indices.
 * @return Returns -1 if the counter cannot be advanced further; otherwise, returns the index of the counter that was changed.
 */
long advance_values(double *value, long *value_index, double *initial, double *step, long n_values,
                    long *counter, long *max_count, long n_indices) {
  long i, counter_changed;

  if ((counter_changed = advance_counter(counter, max_count, n_indices)) < 0)
    return (-1);

  for (i = 0; i < n_values; i++)
    value[i] = initial[i] + counter[value_index[i]] * step[i];
  return (counter_changed);
}

/**
 * @brief Advances the counter array based on maximum counts.
 *
 * @param counter Pointer to the counter array to be advanced.
 * @param max_count Pointer to the array of maximum counts.
 * @param n_indices Number of indices in the counter.
 * @return Returns -1 if all counters have reached their maximum; otherwise, returns the index that was incremented.
 */
long advance_counter(long *counter, long *max_count, long n_indices) {
  long i;

  for (i = 0; i < n_indices; i++)
    if (counter[i] != (max_count[i] - 1))
      break;
  if (i == n_indices)
    return (-1);

  for (i = 0; i < n_indices; i++) {
    if (counter[i] < (max_count[i] - 1)) {
      counter[i]++;
      break;
    } else {
      counter[i] = 0;
    }
  }
  return (i);
}
