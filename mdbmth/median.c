/**
 * @file median.c
 * @brief Computes statistical measures such as median, percentiles, average, and middle values.
 *
 * This file contains functions to compute median, percentiles, averages, and the middle value of datasets.
 * See also the find_XX() routines in rowmedian.c which return the position of the median and other statistics.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, Y. Wang
 */

#include "mdb.h"

/**
 * @brief Computes the median of an array of doubles.
 *
 * @param value Pointer to store the computed median value.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long compute_median(double *value, double *x, long n) {
  static double *data = NULL;
  static long last_n = 0;
  long i;

  if (n <= 0)
    return 0;
  if (n > last_n) {
    data = trealloc(data, sizeof(*data) * n);
    last_n = n;
  }
  for (i = 0; i < n; i++)
    data[i] = x[i];
  qsort((void *)data, n, sizeof(*data), double_cmpasc);
  *value = data[n / 2];
  return 1;
}

/**
 * @brief Computes a specific percentile of an array of doubles.
 *
 * @param value Pointer to store the computed percentile value.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param percentile The desired percentile to compute (0-100).
 * @return Returns 1 on success, 0 on failure.
 */
long compute_percentile(double *value, double *x, long n, double percentile) {
  static double *data = NULL;
  static long last_n = 0;
  long i;

  if (n <= 0 || percentile < 0 || percentile > 100)
    return 0;
  if (n > last_n) {
    data = trealloc(data, sizeof(*data) * n);
    last_n = n;
  }
  for (i = 0; i < n; i++)
    data[i] = x[i];
  qsort((void *)data, n, sizeof(*data), double_cmpasc);
  *value = data[(long)((n - 1) * (percentile / 100.0))];
  return 1;
}

/**
 * @brief Computes multiple percentiles of an array of doubles.
 *
 * @param position Pointer to the array to store the computed percentile values.
 * @param percent Pointer to the array of percentiles to compute (each value between 0-100).
 * @param positions Number of percentiles to compute.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long compute_percentiles(double *position, double *percent, long positions, double *x, long n) {
  static double *data = NULL;
  static long last_n = 0;
  long ip;

  if (n <= 0 || positions <= 0)
    return 0;
  if (n > last_n) {
    data = trealloc(data, sizeof(*data) * n);
    last_n = n;
  }
  memcpy((char *)data, (char *)x, sizeof(*x) * n);
  qsort((void *)data, n, sizeof(*data), double_cmpasc);
  for (ip = 0; ip < positions; ip++)
    position[ip] = data[(long)((n - 1) * (percent[ip] / 100.0))];
  return 1;
}

/**
 * @brief Computes multiple percentiles of an array of doubles, considering only flagged elements.
 *
 * @param position Pointer to the array to store the computed percentile values.
 * @param percent Pointer to the array of percentiles to compute (each value between 0-100).
 * @param positions Number of percentiles to compute.
 * @param x Pointer to the array of doubles.
 * @param keep Pointer to the array of flags indicating which elements to include.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long compute_percentiles_flagged(double *position, double *percent, long positions, double *x, int32_t *keep, int64_t n) {
  static double *data = NULL;
  static int64_t last_n = 0;
  int64_t ip, jp, count;

  if (n <= 0 || positions <= 0)
    return 0;
  for (ip=count=0; ip<n; ip++) 
    if (keep[ip])
      count++;
  if (count > last_n) {
    data = trealloc(data, sizeof(*data) * count);
    last_n = count;
  }
  for (ip=jp=0; ip<n; ip++)
    if (keep[ip])
      data[jp++] = x[ip];
  qsort((void *)data, count, sizeof(*data), double_cmpasc);
  for (ip = 0; ip < positions; ip++)
    position[ip] = data[(long)((count - 1) * (percent[ip] / 100.0))];
  return 1;
}

/**
 * @brief Computes the average of an array of doubles.
 *
 * @param value Pointer to store the computed average value.
 * @param data Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long compute_average(double *value, double *data, int64_t n) {
  double sum;
  int64_t i;

  if (n <= 0)
    return 0;

  for (i = sum = 0; i < n; i++)
    sum += data[i];
  *value = sum / n;
  return 1;
}

/**
 * @brief Computes the middle value between the minimum and maximum of an array of doubles.
 *
 * @param value Pointer to store the computed middle value.
 * @param data Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long compute_middle(double *value, double *data, long n) {
  double min, max;
  if (n <= 0)
    return 0;

  if (!find_min_max(&min, &max, data, n))
    return 0;
  *value = (min + max) / 2;
  return 1;
}

/**
 * @brief Approximates multiple percentiles of an array of doubles using histogram bins.
 *
 * @param position Pointer to the array to store the computed percentile positions.
 * @param percent Pointer to the array of percentiles to compute (each value between 0-100).
 * @param positions Number of percentiles to compute.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param bins Number of histogram bins to use for approximation.
 * @return Returns 1 on success, 0 on failure.
 */
long approximate_percentiles(double *position, double *percent, long positions, double *x, long n,
                             long bins) {
  double *hist, *cdf, xMin, xMax, xCenter, xRange;
  long i, j, k;
  if (bins < 2 || positions <= 0 || n <= 0)
    return 0;
  if (!(hist = malloc(sizeof(*hist) * bins)))
    return 0;
  find_min_max(&xMin, &xMax, x, n);
  xCenter = (xMax + xMin) / 2;
  xRange = (xMax - xMin) * (1 + 1. / bins) / 2;
  xMin = xCenter - xRange;
  xMax = xCenter + xRange;
  make_histogram(hist, bins, xMin, xMax, x, n, 1);

  cdf = hist;
  for (i = 1; i < bins; i++)
    cdf[i] += cdf[i - 1];
  for (i = 0; i < bins; i++)
    cdf[i] /= cdf[bins - 1];

  for (j = 0; j < positions; j++) {
    for (i = k = 0; i < bins; i++) {
      if (cdf[i] < percent[j] / 100.0)
        k = i;
      else
        break;
    }
    position[j] = xMin + (k * (xMax - xMin)) / bins;
  }
  free(hist);
  return 1;
}
