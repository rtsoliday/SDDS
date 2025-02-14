/**
 * @file gy.c
 * @brief Implements the integral of K_{5/3}(t) multiplied by y^n from y to infinity.
 *
 * This file provides the implementation of gy(), which computes:
 *         inf.
 *  GY = y^n ∫  K_{5/3}(t) dt
 *         y
 * The integral evaluation is based on methods described by V. O. Kostroun and uses 
 * exponential and hyperbolic function evaluations. It is generally reliable up to 
 * several decimal places for a range of inputs.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author R. Dejus, H. Shang, R. Soliday, M. Borland
 */

#include "mdb.h"
#include <math.h>
#undef EPS
#define EPS 1.0e-8
#define NY 5.0 / 3.0

/**
 * @brief Compute the integral of K_{5/3}(t) scaled by y^n from y to infinity.
 *
 * This function calculates the integral:
 *   GY = y^n ∫ from t=y to t=∞ of K_{5/3}(t) dt.
 * It uses a summation approach with an iterative increment (dt) and stops when 
 * subsequent terms are sufficiently small compared to the current sum. 
 * 
 * @param n The exponent applied to y.
 * @param y The lower limit of the integral.
 * @return The computed integral value GY.
 */
double gy(long n, double y) {
  double p, sum, term, dt, gy;

  p = 1.0;
  sum = 0.0;
  dt = 0.1;
  term = exp(-y * cosh(p * dt)) * cosh(NY * p * dt) / cosh(p * dt);
  while (term > EPS * sum) {
    sum += term;
    p = p + 1;
    term = exp(-y * cosh(p * dt)) * cosh(NY * p * dt) / cosh(p * dt);
  }
  gy = pow(y, n) * dt * (sum + 0.5 * exp(-y));
  return gy;
}
