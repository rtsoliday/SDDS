/**
 * @file rowmedian.c
 * @brief Computes statistical measures such as median, percentiles, average, and middle values for arrays.
 *
 * This file contains functions to compute the median, specific percentiles, the average, and the middle value of
 * datasets. It also includes functions to compute these statistics for specific rows in a 2D array. These
 * functions return the index of the closest point to the computed statistic. Requires libsort.a routines.
 *
 * See also: median.c (doesn't require libsort.a, but doesn't return index).
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
 * @brief Finds the median value of an array of doubles and returns the index of the median.
 *
 * @param value Pointer to store the computed median value.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the index of the median element on success, -1 on failure.
 */
long find_median(double *value, double *x, long n) {
  static double **data = NULL;
  static long last_n = 0;
  long i;

  if (n <= 0)
    return (-1);
  if (n > last_n) {
    if (data)
      free_zarray_2d((void **)data, last_n, 2);
    data = (double **)zarray_2d(sizeof(**data), n, 2);
    last_n = n;
  }

  for (i = 0; i < n; i++) {
    data[i][0] = x[i];
    data[i][1] = i;
  }
  set_up_row_sort(0, 2, sizeof(**data), double_cmpasc);
  qsort((void *)data, n, sizeof(*data), row_compare);

  *value = data[n / 2][0];
  i = data[n / 2][1];

  return (i);
}

/**
 * @brief Finds a specific percentile of an array of doubles and returns the index of the percentile.
 *
 * @param value Pointer to store the computed percentile value.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param percentile The desired percentile to compute (0-100).
 * @return Returns the index of the percentile element on success, -1 on failure.
 */
long find_percentile(double *value, double *x, long n, double percentile) {
  static double **data = NULL;
  static long last_n = 0;
  long i;

  if (n <= 0)
    return (-1);
  if (percentile < 0 || percentile > 100)
    return -1;
  if (n > last_n) {
    if (data)
      free_zarray_2d((void **)data, last_n, 2);
    data = (double **)zarray_2d(sizeof(**data), n, 2);
    last_n = n;
  }

  for (i = 0; i < n; i++) {
    data[i][0] = x[i];
    data[i][1] = i;
  }
  set_up_row_sort(0, 2, sizeof(**data), double_cmpasc);
  qsort((void *)data, n, sizeof(*data), row_compare);

  *value = data[(long)((n - 1) * (percentile / 100.0))][0];
  i = data[(long)((n - 1) * (percentile / 100.0))][1];

  return (i);
}

/**
 * @brief Finds the median value of a specific row in a 2D array of doubles and returns the index of the median.
 *
 * @param value Pointer to store the computed median value.
 * @param x Pointer to the 2D array of doubles.
 * @param index The row index for which the median is to be computed.
 * @param n Number of elements in the row.
 * @return Returns the index of the median element in the specified row on success, -1 on failure.
 */
long find_median_of_row(double *value, double **x, long index, long n) {
  static double **data = NULL;
  static long last_n = 0;
  long i;

  if (index < 0 && n <= 0)
    return (-1);

  if (n > last_n) {
    if (data)
      free_zarray_2d((void **)data, last_n, 2);
    data = (double **)zarray_2d(sizeof(**data), n, 2);
    last_n = n;
  }

  for (i = 0; i < n; i++) {
    data[i][0] = x[i][index];
    data[i][1] = i;
  }
  set_up_row_sort(0, 2, sizeof(**data), double_cmpasc);
  qsort((void *)data, n, sizeof(*data), row_compare);

  *value = data[n / 2][0];
  i = data[n / 2][1];

  return (i);
}

/**
 * @brief Finds the average of an array of doubles and returns the index of the element closest to the average.
 *
 * @param value Pointer to store the computed average value.
 * @param data Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the index of the element closest to the average on success, -1 on failure.
 */
long find_average(double *value, double *data, long n) {
  double sum, min_dist, dist;
  long i, i_best;

  if (n <= 0)
    return (-1);

  for (i = sum = 0; i < n; i++)
    sum += data[i];
  sum /= n;

  min_dist = DBL_MAX;
  i_best = -1;
  for (i = 0; i < n; i++)
    if ((dist = fabs(data[i] - sum)) < min_dist) {
      min_dist = dist;
      i_best = i;
    }
  *value = sum;
  return (i_best);
}

/**
 * @brief Finds the middle value between the minimum and maximum of an array of doubles and returns the index of the closest element.
 *
 * @param value Pointer to store the computed middle value.
 * @param data Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the index of the element closest to the middle value on success, -1 on failure.
 */
long find_middle(double *value, double *data, long n) {
  double target, min_dist, dist, min, max;
  long i, i_best;

  if (n <= 0)
    return (-1);

  if (!find_min_max(&min, &max, data, n))
    return (-1);
  target = (min + max) / 2;

  min_dist = DBL_MAX;
  i_best = -1;
  for (i = 0; i < n; i++)
    if ((dist = fabs(data[i] - target)) < min_dist) {
      min_dist = dist;
      i_best = i;
    }
  *value = target;
  return (i_best);
}
