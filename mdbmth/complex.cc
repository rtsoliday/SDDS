/**
 * @file complex.cc
 * @brief Implementation of complex number functions.
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

#include <complex>
#include "mdb.h"

/**
 * @brief Computes the complex error function of a given complex number.
 *
 * @param z The complex number input.
 * @param flag Pointer to a flag variable to store computation status.
 * @return The complex error function value of z.
 */
std::complex<double> complexErf(std::complex<double> z, long *flag) {
  double xi, yi;
  double u = 0, v = 0;
  long lflag = 0;
  xi = z.real();
  yi = z.imag();
  wofz(&xi, &yi, &u, &v, &lflag);
  *flag = lflag;
  return std::complex<double>(u, v);
}

/**
 * @brief Computes the complex exponential of an imaginary number.
 *
 * @param p The real number representing the imaginary part.
 * @return The complex exponential of the input.
 */
std::complex<double> cexpi(double p) {
  std::complex<double> a;
  a = cos(p) + std::complex<double>(0, 1) * sin(p);
  return (a);
}

/**
 * @brief Raises a complex number to an integer power.
 *
 * @param a The base complex number.
 * @param n The exponent (integer).
 * @return The complex number a raised to the power n.
 */
std::complex<double> cipowr(std::complex<double> a, int n) {
  int i;
  std::complex<double> p(1, 0);

  if (n >= 0) {
    for (i = 0; i < n; i++)
      p = p * a;
    return (p);
  }
  a = p / a;
  return (cipowr(a, -n));
}

/**
 * @brief Multiplies two complex numbers.
 *
 * @deprecated These routines are obsolete, really, but some code uses them.
 * 
 * @param r0 Pointer to store the real part of the result.
 * @param i0 Pointer to store the imaginary part of the result.
 * @param r1 Real part of the first complex number.
 * @param i1 Imaginary part of the first complex number.
 * @param r2 Real part of the second complex number.
 * @param i2 Imaginary part of the second complex number.
 */
void complex_multiply(
  double *r0, double *i0, /* result */
  double r1, double i1,
  double r2, double i2) {
  double tempr;

  tempr = r1 * r2 - i1 * i2;
  *i0 = r1 * i2 + i1 * r2;
  *r0 = tempr;
}

/**
 * @brief Divides two complex numbers.
 *
 * @deprecated These routines are obsolete, really, but some code uses them.
 *
 * @param r0 Pointer to store the real part of the result.
 * @param i0 Pointer to store the imaginary part of the result.
 * @param r1 Real part of the numerator complex number.
 * @param i1 Imaginary part of the numerator complex number.
 * @param r2 Real part of the denominator complex number.
 * @param i2 Imaginary part of the denominator complex number.
 * @param threshold The threshold to prevent division by very small numbers.
 */
void complex_divide(
  double *r0, double *i0, /* result */
  double r1, double i1,
  double r2, double i2,
  double threshold) {
  double tempr, denom;

  if ((denom = sqr(r2) + sqr(i2)) < threshold)
    denom = threshold;
  i2 = -i2;
  tempr = (r1 * r2 - i1 * i2) / denom;
  *i0 = (r1 * i2 + i1 * r2) / denom;
  *r0 = tempr;
}
