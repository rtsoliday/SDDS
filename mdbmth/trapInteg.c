/**
 * @file trapInteg.c
 * @brief Provides functions for performing trapezoidal integration on data sets.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday
 */

#include "mdb.h"

/**
 * @brief Computes the integral of a dataset using the trapezoidal rule.
 *
 * This function calculates the integral of the given data points using the trapezoidal rule.
 *
 * @param x Pointer to the array of x-values.
 * @param y Pointer to the array of y-values.
 * @param n The number of data points.
 * @param integral Pointer to store the computed integral.
 * @return Returns 1 on success, 0 on failure.
 */
long trapazoidIntegration(double *x, double *y, long n, double *integral) {
  double sum;
  long i;

  if (!x || !y || !integral || n <= 1)
    return 0;
  sum = y[n - 1] * x[n - 1] - y[0] * x[0];
  for (i = 0; i < n - 1; i++)
    sum += y[i] * x[i + 1] - y[i + 1] * x[i];
  *integral = sum / 2;
  return 1;
}

/**
 * @brief Computes the integral as a function of x using the trapezoidal rule.
 *
 * This function calculates the integral at each x-value using the trapezoidal rule.
 *
 * @param x Pointer to the array of x-values.
 * @param y Pointer to the array of y-values.
 * @param n The number of data points.
 * @param integral Pointer to store the computed integral values.
 * @return Returns 1 on success, 0 on failure.
 */
long trapazoidIntegration1(double *x, double *y, long n, double *integral) {
  long i;

  if (!x || !y || !integral || n <= 1)
    return 0;
  integral[0] = 0;
  for (i = 1; i < n; i++)
    integral[i] = integral[i - 1] + (y[i] + y[i - 1]) * (x[i] - x[i - 1]) / 2;
  return 1;
}
