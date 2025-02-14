/**
 * @file lsfBasisFns.c
 * @brief Basis functions for Least Squares Fits (LSFs) using ordinary and Chebyshev polynomials.
 *
 * This file provides a set of functions for evaluating basis functions and their derivatives
 * for least squares fitting. It includes functions to set and retrieve scaling offsets, 
 * as well as polynomial and Chebyshev polynomial basis functions and their derivatives.
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

static double x_offset = 0;
static double x_scale = 1;

/**
 * @brief Set the offset applied to the input argument of basis functions.
 *
 * This function updates the global variable used to shift the input argument before 
 * evaluating polynomial or Chebyshev functions.
 *
 * @param offset The new offset to apply to the argument.
 */
void set_argument_offset(double offset) { x_offset = offset; }

/**
 * @brief Set the scale factor applied to the input argument of basis functions.
 *
 * This function updates the global variable used to scale the input argument before 
 * evaluating polynomial or Chebyshev functions. It ensures the scale factor is not zero.
 *
 * @param scale The new scale factor for the argument.
 */
void set_argument_scale(double scale) {
  if (!(x_scale = scale))
    bomb("argument scale factor is zero", NULL);
}

/**
 * @brief Get the current argument offset applied before function evaluations.
 *
 * @return The current argument offset.
 */
double get_argument_offset() { return x_offset; }

/**
 * @brief Get the current argument scale factor used before function evaluations.
 *
 * @return The current argument scale factor.
 */
double get_argument_scale() { return x_scale; }

/**
 * @brief Evaluate the Chebyshev polynomial of the first kind T_n(x).
 *
 * Given x and an order n, this function returns T_n((x - x_offset) / x_scale).
 * If x is out of the domain [-1,1], it is clipped to ±1 before evaluation.
 *
 * @param x The point at which to evaluate the Chebyshev polynomial.
 * @param n The order of the Chebyshev polynomial.
 * @return The value of T_n(x).
 */
double tcheby(double x, long n) {
  x = (x - x_offset) / x_scale;
  if (x > 1 || x < -1) {
    /*  fprintf(stderr, "warning: argument %e is out of range for tcheby()\n",
     * x); */
    x = SIGN(x);
  }
  return (cos(n * acos(x)));
}

/**
 * @brief Evaluate the derivative of the Chebyshev polynomial T_n(x).
 *
 * This function returns d/dx [T_n((x - x_offset)/x_scale)].
 * If x is out of the domain [-1,1], it is clipped to ±1 before evaluation.
 *
 * @param x The point at which to evaluate the derivative of T_n.
 * @param n The order of the Chebyshev polynomial.
 * @return The derivative dT_n/dx at the given x.
 */
double dtcheby(double x, long n) {
  x = (x - x_offset) / x_scale;
  if (x > 1 || x < -1) {
    /* fprintf(stderr, "warning: argument %e is out of range for tcheby()\n",
     * x); */
    x = SIGN(x);
  }
  if (x != 1 && x != -1)
    return (n * sin(n * acos(x)) / sqrt(1 - sqr(x)));
  return (1.0 * n * n);
}

/**
 * @brief Evaluate a power function x^n.
 *
 * This function returns ( (x - x_offset)/x_scale )^n.
 *
 * @param x The point at which to evaluate the power.
 * @param n The exponent.
 * @return The value of ((x - x_offset)/x_scale)^n.
 */
double ipower(double x, long n) {
  x = (x - x_offset) / x_scale;
  return (ipow(x, n));
}

/**
 * @brief Evaluate the derivative of x^n.
 *
 * This function returns d/dx [ (x - x_offset)/x_scale ]^n = n * ((x - x_offset)/x_scale)^(n-1) / x_scale.
 *
 * @param x The point at which to evaluate the derivative.
 * @param n The exponent.
 * @return The derivative of the power function at the given x.
 */
double dipower(double x, long n) {
  x = (x - x_offset) / x_scale;
  return (n * ipow(x, n - 1));
}

/**
 * @brief Evaluate a sum of basis functions.
 *
 * Given a pointer to a function that evaluates a basis function fn(x,order),
 * this function computes the weighted sum of these functions at x0 using the provided coefficients.
 *
 * @param fn A pointer to the basis function to evaluate (fn(x, order)).
 * @param coef An array of coefficients for each basis function.
 * @param order An array of orders corresponding to each coefficient.
 * @param n_coefs The number of coefficients (and basis functions).
 * @param x0 The point at which to evaluate the sum.
 * @return The computed sum of the basis functions.
 */
double eval_sum(double (*fn)(double x, long ord), double *coef, int32_t *order,
                long n_coefs, double x0) {
  double sum = 0;
  long i;

  for (i = sum = 0; i < n_coefs; i++)
    sum += (fn)(x0, order[i]) * coef[i];
  return (sum);
}
