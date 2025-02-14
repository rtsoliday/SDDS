/**
 * @file lincorr.c
 * @brief Functions for computing linear correlation coefficients and their significance.
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
 * @brief Compute the linear correlation coefficient for two data sets.
 *
 * Given two arrays of data and optional acceptance arrays, this function calculates
 * the linear correlation coefficient (r) between the two data sets.
 * Invalid values (NaN or Inf) and unaccepted entries are skipped.
 *
 * @param data1 First data array.
 * @param data2 Second data array.
 * @param accept1 Optional acceptance array for data1, may be NULL.
 * @param accept2 Optional acceptance array for data2, may be NULL.
 * @param rows Number of elements in each data array.
 * @param count Pointer to a long to store the number of valid paired samples used.
 * @return The linear correlation coefficient (between -1 and 1).
 */
double linearCorrelationCoefficient(double *data1, double *data2,
                                    short *accept1, short *accept2,
                                    long rows, long *count) {
  double sum1[2] = {0, 0}, sum2[2] = {0, 0}, sum12 = 0;
  double d1, d2, r;
  long i;
  long count1;

  count1 = 0;
  for (i = 0; i < rows; i++) {
    if (isnan(data1[i]) || isnan(data2[i]) || isinf(data1[i]) || isinf(data2[i]))
      continue;
    if ((accept1 && !accept1[i]) || (accept2 && !accept2[i]))
      continue;
    count1 += 1;
    sum1[0] += data1[i];
    sum1[1] += data1[i] * data1[i];
    sum2[0] += data2[i];
    sum2[1] += data2[i] * data2[i];
    sum12 += data1[i] * data2[i];
  }
  if (count)
    *count = count1;
  d1 = count1 * sum1[1] - sum1[0] * sum1[0];
  d2 = count1 * sum2[1] - sum2[0] * sum2[0];
  if (d1 <= 0 || d2 <= 0)
    return 0.0;
  if ((d1 *= d2) <= 0)
    return 0.0;
  r = (count1 * sum12 - sum1[0] * sum2[0]) / sqrt(d1);
  if (r < -1)
    r = -1;
  else if (r > 1)
    r = 1;
  return r;
}

/**
 * @brief Compute the statistical significance of a linear correlation coefficient.
 *
 * Given a correlation coefficient and the sample size, this function returns the
 * significance level, i.e., the probability of observing such a correlation by chance.
 *
 * @param r The linear correlation coefficient.
 * @param rows The number of samples used to compute r.
 * @return The significance level as a probability (0 to 1).
 */
double linearCorrelationSignificance(double r, long rows) {
  if (rows < 2)
    return 1.0;
  if ((r = fabs(r)) > 1)
    r = 1;
  return rSigLevel(r, rows - 2);
}

/**
 * @brief Compute the linear correlation coefficient between two data sets with a given shift.
 *
 * Similar to linearCorrelationCoefficient(), but applies a time shift (lag) between the data sets.
 * One array is shifted relative to the other, and only overlapping values are considered.
 *
 * @param data1 First data array.
 * @param data2 Second data array.
 * @param accept1 Optional acceptance array for data1, may be NULL.
 * @param accept2 Optional acceptance array for data2, may be NULL.
 * @param rows Number of elements in each data array.
 * @param count Pointer to a long to store the number of valid paired samples used after shifting.
 * @param shift The integer shift to apply (positive or negative).
 * @return The linear correlation coefficient (between -1 and 1) for the shifted data.
 */
double shiftedLinearCorrelationCoefficient(double *data1, double *data2,
                                           short *accept1, short *accept2,
                                           long rows, long *count, long shift) {
  double sum1[2] = {0, 0}, sum2[2] = {0, 0}, sum12 = 0;
  double d1, d2, r;
  long i, i1, i2, is;

  if (shift > 0) {
    i1 = shift;
    i2 = rows;
  } else {
    i1 = 0;
    i2 = rows + shift;
  }
  *count = 0;
  for (i = i1; i < i2; i++) {
    is = i - shift;
    if (is < 0 || is >= rows) {
      fprintf(stderr, "shift limits set incorrectly\n");
      exit(1);
    }
    if (isnan(data1[i]) || isnan(data2[is]) || isinf(data1[i]) || isinf(data2[is]))
      continue;
    if ((accept1 && !accept1[i]) || (accept2 && !accept2[is]))
      continue;
    *count += 1;
    sum1[0] += data1[i];
    sum1[1] += data1[i] * data1[i];
    sum2[0] += data2[is];
    sum2[1] += data2[is] * data2[is];
    sum12 += data1[i] * data2[is];
  }
  if (!*count)
    return 0.0;
  d1 = (*count) * sum1[1] - sum1[0] * sum1[0];
  d2 = (*count) * sum2[1] - sum2[0] * sum2[0];
  if (d1 <= 0 || d2 <= 0)
    return 0.0;
  if ((d1 *= d2) <= 0)
    return 0.0;
  r = ((*count) * sum12 - sum1[0] * sum2[0]) / sqrt(d1);
  if (r < -1)
    r = -1;
  else if (r > 1)
    r = 1;
  return r;
}
