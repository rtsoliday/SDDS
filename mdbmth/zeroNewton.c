/**
 * @file zeroNewton.c
 * @brief Implements the zeroNewton function for finding zeros of a function using Newton's method with numerical derivatives.
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
 * @brief Finds the zero of a function using Newton's method with numerical derivative computation.
 *
 * This function attempts to find a value `x` such that `fn(x)` is approximately equal to the given `value`.
 * It employs Newton's iterative method, where the derivative is estimated numerically using finite differences.
 *
 * @param fn Pointer to the function for which the zero is to be found.
 * @param value The target value to solve for, i.e., find `x` such that `fn(x) = value`.
 * @param x_i Initial guess for the independent variable.
 * @param dx Increment used for numerical derivative computation.
 * @param n_passes Number of iterations to perform.
 * @param _zero Acceptable tolerance for the zero, determining when to stop the iterations.
 *
 * @return The `x` value where `fn(x)` is approximately equal to `value`, or `DBL_MAX` if no zero is found within the iteration limit.
 */
double zeroNewton(
  double (*fn)(), /* pointer to function to be zeroed */
  double value,   /* value of function to find */
  double x_i,     /* initial value for independent variable */
  double dx,      /* increment for taking numerical derivatives */
  long n_passes,  /* number of times to iterate */
  double _zero    /* value acceptably close to true zero */
) {
  double f1, f2;
  double x1, x2;
  double dfdx;
  long i;

  x1 = x2 = x_i;
  for (i = 0; i < n_passes; i++) {
    if (fabs(f1 = (*fn)(x1)-value) < _zero)
      return (x1);
    if (i == n_passes - 1)
      return ((x1 + x2) / 2);
    f2 = (*fn)(x2 = x1 + dx) - value;
    dfdx = ((f2 - f1) / dx);
    x1 += -f1 / dfdx;
  }
  return (DBL_MAX); /* shouldn't happen */
}
