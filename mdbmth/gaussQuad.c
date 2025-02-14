/**
 * @file gaussQuad.c
 * @brief Implements recursive Gaussian quadrature for numerical integration.
 *
 * This file contains the implementation of the recursive Gaussian quadrature
 * technique for integrating functions numerically. The primary function
 * `gaussianQuadrature` divides the integration interval into panels and
 * recursively refines the estimate to achieve the desired error tolerance.
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

#include <math.h>
#include "mdb.h"

#define _CGQ .57735026918962576451
#define gauss_quad(fn, z, a, w) (((*fn)(z + a) + (*fn)(a - z)) * w)

#define MAXSTACK 16384

/**
 * @brief Integrate a function using recursive Gaussian quadrature.
 *
 * Performs numerical integration of the specified function over the interval [a, b]
 * using a recursive Gaussian quadrature technique. The interval is initially divided
 * into `n` panels, and each panel is recursively subdivided until the desired
 * fractional error `err` is achieved.
 *
 * @param fn Pointer to the function to be integrated.
 * @param a Lower limit of integration.
 * @param b Upper limit of integration.
 * @param n Initial number of panels to divide the interval into.
 * @param err Fractional error permissible on any quadrature.
 * @param result Pointer to a double where the result of the integration will be stored.
 * @return The total number of function evaluations performed, or -1 if an error occurs.
 */
long gaussianQuadrature(
  double (*fn)(),     /* pointer to function to be integrated */
  double a, double b, /* upper, lower limits of integration */
  long n,             /* number of panels to start with */
  double err,         /* fractional error permissible on any quadrature */
  double *result) {
  register long s; /* stack pointer for panel limits */
  double ave, i0, i1, i2, i3, idiv;
  long totalEvals;
  double d_ab, i_tot, d_ab2;
  double ab, z, width;
  /* stacks of upper,lower limits, and integral values from two
   * point quadratures on each interval */
  double a_stack[MAXSTACK];
  double b_stack[MAXSTACK];
  double i_stack[MAXSTACK];

  if (n > MAXSTACK)
    return -1;

  d_ab = (b - a) / n;
  d_ab2 = d_ab / 2;
  totalEvals = 0;
  for (s = 0; s < n; s++) {
    b = b_stack[s] = (a_stack[s] = a) + d_ab;
    ave = a + d_ab2;
    z = d_ab2 * _CGQ;
    i_stack[s] = gauss_quad(fn, z, ave, d_ab2);
    totalEvals += 2;
    a = b;
  }

  s = n - 1;
  i_tot = 0;
  while (s >= 0) {
    if (s == MAXSTACK)
      return -1;
    a = a_stack[s];
    b = b_stack[s];
    ab = (a + b) / 2;
    i0 = i_stack[s];

    ave = (a + ab) / 2;
    z = (width = (ab - a) / 2) * _CGQ;
    i1 = gauss_quad(fn, z, ave, width);
    totalEvals += 2;

    ave = (ab + b) / 2;
    z = (width = (b - ab) / 2) * _CGQ;
    i2 = gauss_quad(fn, z, ave, width);
    totalEvals += 2;

    i3 = idiv = i1 + i2;
    if (i3 == 0)
      idiv = i0;
    if (idiv != 0) {
      if (fabs((i3 - i0) / idiv) > err) {
        b_stack[s] = ab;
        i_stack[s] = i1;
        a_stack[++s] = ab;
        b_stack[s] = b;
        i_stack[s] = i2;
      } else {
        i_tot += i3;
        s--;
      }
    } else {
      i_tot += i3;
      s--;
    }
  }
  *result = i_tot;
  return totalEvals;
}
