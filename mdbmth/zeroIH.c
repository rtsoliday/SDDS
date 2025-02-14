/**
 * @file zeroIH.c
 * @brief Implements the zeroIntHalve function for finding zeros of a function using interval halving.
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

#include <math.h>
#include <stdio.h>

#define sign(x) ((x) > 0 ? 1 : ((x) == 0 ? 0 : -1))

/**
 * @brief Finds the zero of a function within a specified interval using interval halving.
 *
 * This function attempts to find a value `x` such that `fn(x)` is approximately equal to the given `value`.
 * It employs the bisection method to iteratively narrow down the interval where the zero lies within `[x_i, x_f]`.
 *
 * @param fn Pointer to the function for which the zero is to be found.
 * @param value The target value to solve for, i.e., find `x` such that `fn(x) = value`.
 * @param x_i Initial value of the independent variable (start of the interval).
 * @param x_f Final value of the independent variable (end of the interval).
 * @param dx Step size to use when searching the interval.
 * @param _zero Acceptable tolerance for the zero, determining when to stop the halving process.
 *
 * @return The `x` value where `fn(x)` is approximately equal to `value`, or `x_f + dx` if no zero is found within the interval.
 */
double zeroIntHalve(
  double (*fn)(), /* pointer to function to be zeroed */
  double value,   /* solve for fn=value */
  double x_i,     /* initial, final values for independent variable */
  double x_f,
  double dx,    /* size of steps in independent variable */
  double _zero) /* value acceptably close to true zero */
{
  double xa, xb, xm, x_b;
  double fa, fb, fm;
  double f_abs, f_bdd;
  long s_fa, s_fb, s_fm;
  /*	double zeroIH();*/

  if (dx > (x_f - x_i))
    dx = (x_f - x_i) / 2;

  xa = x_i;
  xb = xa + dx;

  if (xb > x_f)
    xb = x_f;
  if (xa == xb)
    xa = xb - dx;

  fa = (*fn)(xa)-value;
  s_fa = sign(fa);

  while (xb <= x_f) {
    fb = (*fn)(xb)-value;
    s_fb = sign(fb);
    if (s_fb == s_fa) {
      fa = fb;
      xa = xb;
      s_fa = s_fb;
      xb = xb + dx;
    } else {
      /* function has passed through zero */
      /* so halve the interval, etc... */
      f_bdd = 1000 * fabs(fa);
      fm = (*fn)(xm = (xa + xb) / 2) - value;
      s_fm = sign(fm);
      x_b = xb;
      do {
        if (s_fm == 0)
          return (xm);
        else if (s_fm != s_fa) {
          xb = xm;
          fb = fm;
          s_fb = s_fm;
        } else {
          xa = xm;
          fa = fm;
          s_fa = s_fm;
        }
        fm = (*fn)(xm = (xa + xb) / 2) - value;
        s_fm = sign(fm);
        f_abs = fabs(fm);
      } while (f_abs > _zero && f_abs < f_bdd);
      if (f_abs < _zero)
        return (xm);
      /* Function had a tan(x)-like singularity, which
			 * looked like a zero.  So step beyond singularity
			 * and continue search.
			 */
      /*return(zeroIH(fn, x_b, x_f, dx, _zero));*/
      return (zeroIntHalve(fn, value, x_b, x_f, dx, _zero));
    }
  }
  return (x_f + dx);
}
