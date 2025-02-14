/**
 * @file gammai.c
 * @brief Routines for computing the incomplete gamma functions.
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

#if defined(_WIN32)
#  if _MSC_VER <= 1600
#    include "fdlibm.h"
#  endif
#endif

double gammaIncSeries(double a, double x);
double gammaIncCFrac(double a, double x);

#define GAMMAI_ACCURACY 1e-12

/**
 * @brief Compute the regularized lower incomplete gamma function P(a,x).
 *
 * For given parameters a > 0 and x ≥ 0, this function returns the value of P(a,x) = γ(a,x)/Γ(a),
 * where γ(a,x) is the lower incomplete gamma function. Uses series expansion for x < a+1,
 * and a continued fraction expansion otherwise.
 *
 * @param a Shape parameter (a > 0).
 * @param x The upper limit of the integral (x ≥ 0).
 * @return The value of P(a,x), or -1 if inputs are invalid.
 */
double gammaP(double a, double x) {
  if (a <= 0 || x < 0)
    return -1;
  if (x == 0)
    return 0;
  if (x < a + 1) {
    /* use series */
    return gammaIncSeries(a, x);
  } else {
    /* use continued fraction */
    return 1 - gammaIncCFrac(a, x);
  }
}

/**
 * @brief Compute the regularized upper incomplete gamma function Q(a,x).
 *
 * For given parameters a > 0 and x ≥ 0, this function returns Q(a,x) = 1 - P(a,x).
 * It uses a series expansion when x < a+1 and a continued fraction expansion otherwise.
 *
 * @param a Shape parameter (a > 0).
 * @param x The upper limit of the integral (x ≥ 0).
 * @return The value of Q(a,x), or -1 if inputs are invalid.
 */
double gammaQ(double a, double x) {
  if (a <= 0 || x < 0)
    return -1;
  if (x == 0)
    return 0;
  if (x < a + 1) {
    /* use series */
    return 1 - gammaIncSeries(a, x);
  } else {
    /* use continued fraction */
    return gammaIncCFrac(a, x);
  }
}

#define MAX_SERIES 1000

double gammaIncSeries(double a, double x) {
  double term = 0, sum;
  long n;
#if defined(vxWorks)
  fprintf(stderr, "lgamma function not implemented in vxWorks");
  exit(1);
#else
  term = exp(-x) * pow(x, a) / exp(lgamma(a + 1));
#endif
  sum = 0;
  n = 0;
  do {
    sum += term;
    n++;
    term *= x / (a + n);
  } while (term > GAMMAI_ACCURACY && n < MAX_SERIES);
  return (sum + term);
}

double gammaIncCFrac(double a, double x) {
  double A0, B0, A1, B1, A_1, B_1;
  double an, bn, f1, f2, accuracy = 0, factor = 0;
  long m;

#if defined(vxWorks)
  fprintf(stderr, "lgamma function not implemented in vxWorks");
  exit(1);
#else
  accuracy = GAMMAI_ACCURACY / (factor = exp(-x - lgamma(a) + a * log(x)));
#endif
  A0 = 0;
  B0 = 1;
  A_1 = 1;
  B_1 = 0;
  an = 1;
  bn = x + 1 - a;
  A1 = bn * A0 + an * A_1;
  B1 = bn * B0 + an * B_1;
  f2 = A1 / B1;
  m = 1;
  do {
    A_1 = A0;
    B_1 = B0;
    A0 = A1;
    B0 = B1;
    f1 = f2;
    an = -m * (m - a);
    bn += 2;
    A1 = bn * A0 + an * A_1;
    B1 = bn * B0 + an * B_1;
    f2 = A1 / B1;
    if (B1) {
      /* rescale the recurrence to avoid over/underflow */
      A0 /= B1;
      B0 /= B1;
      A1 /= B1;
      B1 = 1;
    }
    m++;
  } while (m < MAX_SERIES && fabs(f1 - f2) > accuracy);
  return factor * f2;
}
