/** 
 * @file ipow.c
 * @brief This file provides the ipow function for computing integer powers of a double.
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
 * @brief Compute x raised to the power p (x^p).
 *
 * Uses straightforward repeated multiplication for efficiency, 
 * which can be faster than using pow() for integer exponents.
 *
 * Special cases:
 * - If x = 0 and p < 0, an error is reported since division by zero occurs.
 * - If p = 0, the result is 1.0.
 * - If p < 0, the result is 1/(x^(-p)).
 *
 * @param[in] x The base value.
 * @param[in] p The integer exponent.
 * @return The computed value of x raised to the power p.
 */
double ipow(const double x, const int64_t p) {
  double hp;
  int64_t n;

  if (x == 0) {
    if (p < 0)
      bomb("Floating divide by zero in ipow().", NULL);
    return (p == 0 ? 1. : 0.);
  }

  if (p < 0)
    return (1. / ipow(x, -p));

  switch (p) {
  case 0:
    return (1.);
  case 1:
    return (x);
  case 2:
    return (x * x);
  case 3:
    hp = x * x;
    return (hp * x);
  case 4:
    hp = x * x;
    return (hp * hp);
  case 5:
    hp = x * x;
    return (hp * hp * x);
  case 6:
    hp = x * x;
    return (hp * hp * hp);
  case 7:
    hp = x * x * x;
    return (hp * hp * x);
  case 8:
    hp = x * x;
    hp = hp * hp;
    return (hp * hp);
  default:
    n = p / 2;
    hp = ipow(x, n);
    switch (p - 2 * n) {
    case 0:
      return (hp * hp);
    case 1:
      return (hp * hp * x);
    }
    break;
  }
  return (0.); /* never actually executed--keeps compiler happy */
}
