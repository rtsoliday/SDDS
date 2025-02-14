/**
 * @file GillMillerIntegration.c
 * @brief Provides the Gill-Miller integration method for numerical integration of functions.
 *
 * This file contains the implementation of the Gill-Miller integration algorithm,
 * which integrates a function f(x) provided as arrays of x and f values.
 * Based on P. E. Gill and G. F. Miller, The Computer Journal, Vol 15, No. 1, 80-83, 1972.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday, N. Kuklev
 */

#include "mdb.h"

/**
 * @brief Performs numerical integration using the Gill-Miller method.
 *
 * This function integrates a set of data points representing a function f(x) using
 * the Gill-Miller integration technique. The integration is performed based on the
 * algorithm described by P. E. Gill and G. F. Miller in their 1972 publication.
 *
 * @param integral Pointer to an array where the computed integral values will be stored.
 * @param error Pointer to an array where the estimated errors of the integral will be stored.
 *              This parameter can be NULL if error estimates are not required.
 * @param f Pointer to an array of y-values representing the function f(x) to be integrated.
 * @param x Pointer to an array of x-values corresponding to the function f(x).
 * @param n The number of data points in the arrays x and f. Must be greater than 1.
 *
 * @return 
 *   - 0 on successful integration.
 *   - 1 if any of the pointers integral, f, or x are NULL.
 *   - 2 if the number of data points n is less than or equal to 1.
 *   - 3 to 8 for various numerical errors encountered during the integration process.
 */
int GillMillerIntegration(double *integral, double *error,
                          double *f, double *x, long n) {
  double e, de1, de2;
  double h1 = 0, h2 = 0, h3 = 0, h4, r1, r2, r3, r4, d1 = 0, d2 = 0, d3 = 0, c, s, integ, dinteg1, dinteg2;
  long i, k;

  if (!integral || !f || !x)
    return 1;
  if (n <= 1)
    return 2;

  integ = e = s = c = r4 = 0;
  n -= 1; /* for consistency with GM paper */

  integral[0] = 0;
  if (error)
    error[0] = 0;

  k = n - 1;
  dinteg2 = 0;
  de2 = 0;

  for (i = 2; i <= k; i++) {
    dinteg1 = 0;
    if (i == 2) {
      h2 = x[i - 1] - x[i - 2];
      if (h2 == 0)
        return 3;
      d3 = (f[i - 1] - f[i - 2]) / h2;
      h3 = x[i] - x[i - 1];
      d1 = (f[i] - f[i - 1]) / h3;
      h1 = h2 + h3;
      if (h1 == 0)
        return 4;
      d2 = (d1 - d3) / h1;
      h4 = x[i + 1] - x[i];
      r1 = (f[i + 1] - f[i]) / h4;
      if ((h4 + h3) == 0)
        return 5;
      r2 = (r1 - d1) / (h4 + h3);
      h1 = h1 + h4;
      if (h1 == 0)
        return 6;
      r3 = (r2 - d2) / h1;
      integ = h2 * (f[0] + h2 * (d3 / 2.0 - h2 * (d2 / 6 - (h2 + 2 * h3) * r3 / 12)));
      s = -ipow3(h2) * (h2 * (3 * h2 + 5 * h4) + 10 * h3 * h1) / 60.0;
      integral[i - 1] = integ;
      if (error)
        error[i - 1] = 0;
    } else {
      h4 = x[i + 1] - x[i];
      if (h4 == 0)
        return 7;
      r1 = (f[i + 1] - f[i]) / h4;
      r4 = h4 + h3;
      if (r4 == 0)
        return 8;
      r2 = (r1 - d1) / r4;
      r4 = r4 + h2;
      if (r4 == 0)
        return 8;
      r3 = (r2 - d2) / r4;
      r4 = r4 + h1;
      if (r4 == 0)
        return 8;
      r4 = (r3 - d3) / r4;
    }

    dinteg1 = h3 * ((f[i] + f[i - 1]) / 2 - h3 * h3 * (d2 + r2 + (h2 - h4) * r3) / 12.0);
    c = ipow3(h3) * (2 * h3 * h3 + 5 * (h3 * (h4 + h2) + 2 * h4 * h2)) / 120;
    de1 = (c + s) * r4;
    s = (i == 2 ? 2 * c + s : c);

    if (i == 2)
      integral[i] = integ + dinteg1 + e + de1;
    else
      integral[i] = integ + dinteg2 + e + de2;
    integ += dinteg1;

    if (error) {
      if (i == 2)
        error[i] = e + de1;
      else
        error[i] = e + de2;
    }
    e += de1;

    dinteg2 = h4 * (f[i + 1] - h4 * (r1 / 2 + h4 * (r2 / 6 + (2 * h3 + h4) * r3 / 12)));
    de2 = s * r4 - ipow3(h4) * r4 * (h4 * (3 * h4 + 5 * h2) + 10 * h3 * (h2 + h3 + h4)) / 60;

    if (i == k) {
      integ += dinteg2;
      e += de2;
    } else {
      h1 = h2;
      h2 = h3;
      h3 = h4;
      d1 = r1;
      d2 = r2;
      d3 = r3;
    }
  }

  integral[i] = integ + e;
  if (error)
    error[i] = e;

  return 0;
}
