/**
 * @file interp.c
 * @brief Implements interpolation functions including linear and Lagrange interpolation.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang, N. Kuklev
 */

#include "mdb.h"

/**
 * @brief Performs simple linear interpolation of data.
 *
 * This function interpolates the value at a given point using linear interpolation.
 * It handles boundary conditions by returning the nearest data point value when the
 * interpolation point is outside the data range.
 *
 * @param f Pointer to the array of function values.
 * @param x Pointer to the array of independent variable values.
 * @param n Number of data points.
 * @param xo The point at which to interpolate.
 * @param warnings Flag to enable or disable warning messages.
 * @param order The order of interpolation.
 * @param returnCode Pointer to a variable to store the return code status.
 * @return The interpolated value at point xo.
 */
double interp(double *f, double *x, long n, double xo, long warnings, long order, long *returnCode) {
  long hi, lo, mid, offset;

  lo = 0;
  hi = n - 1;
  if (lo == hi) {
    if (warnings)
      printf("warning: only one point--returning value for that point\n");
    *returnCode = 0;
    return (f[0]);
  }
  if (x[lo] < x[hi]) { /* indep variable ordered from small to large */
    if (xo < x[lo = 0]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[0], x[n - 1]);
      *returnCode = 0;
      return (f[lo]);
    }
    if (xo > x[hi = n - 1]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[0], x[n - 1]);
      *returnCode = 0;
      return (f[hi]);
    }

    /* do binary search for closest point */
    while ((hi - lo) > 1) {
      mid = (lo + hi) / 2;
      if (xo < x[mid])
        hi = mid;
      else
        lo = mid;
    }
  } else { /* indep variable ordered from large to small */
    if (xo > x[lo = 0]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[n - 1], x[0]);
      *returnCode = 0;
      return (f[lo]);
    }
    if (xo < x[hi = n - 1]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[n - 1], x[0]);
      *returnCode = 0;
      return (f[hi]);
    }

    /* do binary search for closest point */
    while ((hi - lo) > 1) {
      mid = (lo + hi) / 2;
      if (xo > x[mid])
        hi = mid;
      else
        lo = mid;
    }
  }
  /* L.Emery's contribution */
  offset = lo - (order - 1) / 2; /* offset centers the argument in the set of points. */
  offset = MAX(offset, 0);
  offset = MIN(offset, n - order - 1);
  offset = MAX(offset, 0);
  return LagrangeInterp(x + offset, f + offset, order + 1, xo, returnCode);
}

/**
 * @brief Performs Lagrange interpolation of data.
 *
 * This function computes the interpolated value at a given point using Lagrange
 * polynomial interpolation of a specified order.
 *
 * @param x Pointer to the array of independent variable values.
 * @param f Pointer to the array of function values.
 * @param order1 The order of the Lagrange polynomial.
 * @param x0 The point at which to interpolate.
 * @param returnCode Pointer to a variable to store the return code status.
 * @return The interpolated value at point x0.
 */
double LagrangeInterp(double *x, double *f, long order1, double x0, long *returnCode) {
  long i, j;
  double denom, numer, sum;

  for (i = sum = 0; i < order1; i++) {
    denom = 1;
    numer = 1;
    for (j = 0; j < order1; j++) {
      if (i != j) {
        denom *= (x[i] - x[j]);
        numer *= (x0 - x[j]);
        if (numer == 0) {
          *returnCode = 1;
          return f[j];
        }
      }
    }
    if (denom == 0) {
      *returnCode = 0;
      return 0.0;
    }
    sum += f[i] * numer / denom;
  }
  *returnCode = 1;
  return sum;
}

/**
 * @brief Performs interpolation with range control options.
 *
 * This function interpolates the value at a given point with additional control
 * over out-of-range conditions, such as skipping, aborting, warning, wrapping,
 * saturating, or using a specified value.
 *
 * @param f Pointer to the array of function values.
 * @param x Pointer to the array of independent variable values.
 * @param n Number of data points.
 * @param xo The point at which to interpolate.
 * @param belowRange Pointer to structure controlling behavior below the data range.
 * @param aboveRange Pointer to structure controlling behavior above the data range.
 * @param order The order of interpolation.
 * @param returnCode Pointer to a variable to store the return code status.
 * @param M Multiplier to adjust the interpolation condition based on the order.
 * @return The interpolated value at point xo.
 */
double interpolate(double *f, double *x, int64_t n, double xo, OUTRANGE_CONTROL *belowRange,
                   OUTRANGE_CONTROL *aboveRange, long order, unsigned long *returnCode, long M) {
  long code;
  int64_t hi, lo, mid, offset;
  double result, below, above;

  *returnCode = 0;

  lo = 0;
  hi = n - 1;
  if (M > 0) {
    above = f[n - 1];
    below = f[0];
  } else {
    above = f[0];
    below = f[n - 1];
  }
  if ((xo * M > x[hi] * M && M > 0) || (xo * M < x[lo] * M && M < 0)) {
    if (aboveRange->flags & OUTRANGE_SKIP) {
      *returnCode = OUTRANGE_SKIP;
      return above;
    } else if (aboveRange->flags & OUTRANGE_ABORT) {
      *returnCode = OUTRANGE_ABORT;
      return above;
    } else if (aboveRange->flags & OUTRANGE_WARN)
      *returnCode = OUTRANGE_WARN;
    if (aboveRange->flags & OUTRANGE_VALUE) {
      *returnCode |= OUTRANGE_VALUE;
      return aboveRange->value;
    }
    if (aboveRange->flags & OUTRANGE_WRAP) {
      double delta;
      *returnCode |= OUTRANGE_WRAP;
      if ((delta = x[hi] - x[lo]) == 0)
        return f[0];
      while (xo * M > x[hi] * M)
        xo -= delta;
    } else if (aboveRange->flags & OUTRANGE_SATURATE || !(aboveRange->flags & OUTRANGE_EXTRAPOLATE)) {
      *returnCode |= OUTRANGE_SATURATE;
      return above;
    }
  }
  if ((xo * M < x[lo] * M && M > 0) || (xo * M > x[hi] * M && M < 0)) {
    if (belowRange->flags & OUTRANGE_SKIP) {
      *returnCode = OUTRANGE_SKIP;
      return below;
    } else if (belowRange->flags & OUTRANGE_ABORT) {
      *returnCode = OUTRANGE_ABORT;
      return below;
    } else if (belowRange->flags & OUTRANGE_WARN)
      *returnCode = OUTRANGE_WARN;
    if (belowRange->flags & OUTRANGE_VALUE) {
      *returnCode |= OUTRANGE_VALUE;
      return belowRange->value;
    }
    if (belowRange->flags & OUTRANGE_WRAP) {
      double delta;
      *returnCode |= OUTRANGE_WRAP;
      if ((delta = x[hi] - x[lo]) == 0)
        return below;
      while (xo * M < x[lo] * M)
        xo += delta;
    } else if (belowRange->flags & OUTRANGE_SATURATE || !(belowRange->flags & OUTRANGE_EXTRAPOLATE)) {
      *returnCode |= OUTRANGE_SATURATE;
      return below;
    }
  }

  if (lo == hi) {
    if (xo == x[lo]) {
      if (aboveRange->flags & OUTRANGE_WARN || belowRange->flags & OUTRANGE_WARN)
        *returnCode = OUTRANGE_WARN;
    }
    return f[0];
  }

  lo = 0;
  hi = n - 1;
  if (xo * M < x[0] * M)
    hi = 1;
  else if (xo * M > x[n - 1] * M)
    lo = hi - 1;
  else {
    /* do binary search for closest point */
    while ((hi - lo) > 1) {
      mid = (lo + hi) / 2;
      if (xo * M < x[mid] * M)
        hi = mid;
      else
        lo = mid;
    }
  }

  /* L.Emery's contribution */
  if (order > n - 1)
    order = n - 1;
  offset = lo - (order - 1) / 2; /* offset centers the argument in the set of points. */
  offset = MAX(offset, 0);
  offset = MIN(offset, n - order - 1);
  result = LagrangeInterp(x + offset, f + offset, order + 1, xo, &code);
  if (!code)
    bomb("zero denominator in LagrangeInterp", NULL);
  return result;
}

/**
 * @brief Performs interpolation for short integer data types.
 *
 * This function interpolates the value at a given point for short integer data,
 * handling boundary conditions and allowing for different interpolation orders.
 *
 * @param f Pointer to the array of short integer function values.
 * @param x Pointer to the array of independent variable values.
 * @param n Number of data points.
 * @param xo The point at which to interpolate.
 * @param warnings Flag to enable or disable warning messages.
 * @param order The order of interpolation.
 * @param returnCode Pointer to a variable to store the return code status.
 * @param next_start_pos Pointer to store the next starting position for interpolation.
 * @return The interpolated short integer value at point xo.
 */
short interp_short(short *f, double *x, int64_t n, double xo, long warnings, short order,
                   unsigned long *returnCode, long *next_start_pos) {
  int64_t hi, lo, mid, i;
  short value;

  lo = 0;
  hi = n - 1;

  *returnCode = 0;
  /*if the interpolate point is less or equal to the start point, return the value of the start point */
  if (xo <= x[0]) {
    *next_start_pos = 0;
    return f[0];
  }

  /*if the value is in one of the indepent, return the corresponding f value */
  for (i = 0; i < n; i++)
    if (xo == x[i]) {
      *next_start_pos = i;
      return f[i];
    }
  if (lo == hi) {
    if (warnings)
      printf("warning: only one point--returning value for that point\n");
    *returnCode = 0;
    *next_start_pos = lo;
    return (f[0]);
  }
  if (x[lo] < x[hi]) { /* indep variable ordered from small to large */
    if (xo < x[lo = 0]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[0], x[n - 1]);
      *returnCode = 0;
      *next_start_pos = lo;
      return (f[lo]);
    }
    if (xo > x[hi = n - 1]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[0], x[n - 1]);
      *returnCode = 0;
      *next_start_pos = hi;
      return (f[hi]);
    }

    /* do binary search for closest point */
    while ((hi - lo) > 1) {
      mid = (lo + hi) / 2;
      if (xo < x[mid])
        hi = mid;
      else
        lo = mid;
    }
  } else { /* indep variable ordered from large to small */
    if (xo > x[lo = 0]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[n - 1], x[0]);
      *returnCode = 0;
      *next_start_pos = lo;
      return (f[lo]);
    }
    if (xo < x[hi = n - 1]) {
      if (warnings)
        printf("warning: %22.15e outside [%22.15e,%22.15e] (interp)\n",
               xo, x[n - 1], x[0]);
      *returnCode = 0;
      *next_start_pos = hi;
      return (f[hi]);
    }

    /* do binary search for closest point */
    while ((hi - lo) > 1) {
      mid = (lo + hi) / 2;
      if (xo > x[mid])
        hi = mid;
      else
        lo = mid;
    }
  }
  /*remember this position so that one can start from her to speed up the interp of
    other points assume the interping points are sorted.*/
  *next_start_pos = lo;
  *returnCode = 0;
  if (order == -1) {
    /* inherit value from previous point*/
    value = (short)f[lo];
  } else if (order == -2) {
    /* inherit value from next point */
    value = (short)f[hi];
  } else {
    value = (f[hi] - f[lo]) / (xo - x[lo]);
  }
  return value;
}
