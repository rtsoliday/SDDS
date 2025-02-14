/**
 * @file sigLevel.c
 * @brief Routines for Student's t distribution, the F distribution, the linear-correlation coefficient distribution, and Poisson distribution.
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

#if defined(_WIN32) && !defined(__MINGW32__)
#  if _MSC_VER <= 1600
#    include "fdlibm.h"
#    define isnan(x) _isnan(x)
#  endif
#endif

#if defined(vxWorks) /* This may not be very compatible */
#  include <private/mathP.h>
#  define isnan(x) isNan(x)
#endif

/**
 * @brief Computes the probability that a standard normal variable exceeds a given value.
 *
 * This function calculates the probability that a standard normal random variable Z satisfies Z > z0.
 *
 * @param z0 The value for which the probability is computed.
 * @param tails Specifies whether to compute a one-tailed (1) or two-tailed (2) probability.
 *
 * @return The probability that Z exceeds z0. Returns -1 if the number of tails is invalid.
 */
double normSigLevel(double z0, long tails)
{
  if (z0 < 0)
    z0 = -z0;
  if (tails != 1 && tails != 2)
    return -1;
#if defined(vxWorks)
  fprintf(stderr, "erfc function is not implemented on vxWorks\n");
  exit(1);
#else
  return erfc(z0 / SQRT2) / (tails == 1 ? 2 : 1);
#endif
}

/**
 * @brief Computes the probability that a chi-squared variable exceeds a given value.
 *
 * This function calculates the probability that a chi-squared random variable with `nu` degrees of freedom exceeds `ChiSquared0`.
 *
 * @param ChiSquared0 The chi-squared value for which the probability is computed.
 * @param nu The degrees of freedom of the chi-squared distribution.
 *
 * @return The probability that the chi-squared variable exceeds `ChiSquared0` for `nu` degrees of freedom. Returns -1 if `ChiSquared0` is negative.
 */
double ChiSqrSigLevel(double ChiSquared0, long nu)
{
  if (ChiSquared0 < 0)
    return -1;
  return gammaQ(nu / 2.0, ChiSquared0 / 2);
}

/**
 * @brief Computes the probability that the absolute value of a t-distributed variable exceeds a given value.
 *
 * This function calculates the probability that |t| > t0 for a t-distribution with `nu` degrees of freedom.
 *
 * @param t0 The t-value for which the probability is computed.
 * @param nu The degrees of freedom of the t-distribution.
 * @param tails Specifies whether to compute a one-tailed (1) or two-tailed (2) probability.
 *
 * @return The probability that |t| exceeds `t0` for `nu` degrees of freedom. Returns -1 if the number of tails is invalid.
 */
double tTailSigLevel(double t0, long nu, long tails)
{
  if (tails != 1 && tails != 2)
    return -1;
  return betaInc(nu / 2.0, 0.5, nu / (nu + t0 * t0)) / (tails == 1 ? 2 : 1);
}

/**
 * @brief Computes the probability that an F-distributed variable exceeds a given value.
 *
 * This function calculates the probability that an F-distributed random variable with `nu1` and `nu2` degrees of freedom exceeds F0, where F0 is defined as Max(var1, var2) / Min(var1, var2).
 *
 * @param var1 The first variance.
 * @param var2 The second variance.
 * @param nu1 The degrees of freedom associated with `var1`.
 * @param nu2 The degrees of freedom associated with `var2`.
 *
 * @return The probability that F exceeds F0 for `nu1` and `nu2` degrees of freedom.
 */
double FSigLevel(double var1, double var2, long nu1, long nu2)
{
  if (var1 < var2) {
    SWAP_DOUBLE(var1, var2);
    SWAP_LONG(nu1, nu2);
  }
  return betaInc(nu2 / 2.0, nu1 / 2.0, nu2 / (nu2 + nu1 * var1 / var2));
}

/**
 * @brief Computes the probability that the linear correlation coefficient exceeds a given value.
 *
 * This function calculates the probability that the linear correlation coefficient `r` exceeds |r0| for `nu` degrees of freedom.
 *
 * @param r0 The correlation coefficient value.
 * @param nu The degrees of freedom.
 *
 * @return The probability that the linear correlation coefficient exceeds |r0| for `nu` degrees of freedom. Returns -1 if `nu` is less than 2 or if `r0` is not in the valid range.
 */
double rSigLevel(double r0, long nu)
{
  if (r0 < 0)
    r0 = -r0;
  if (nu < 2)
    return -1;
  if (r0 >= 1)
    return 0;
  return tTailSigLevel(r0 * sqrt(nu / (1 - r0 * r0)), nu, 2);
}

/**
 * @brief Computes the probability that a Poisson-distributed random variable exceeds or equals a given value.
 *
 * This function calculates the probability that the number of events `n` is greater than or equal to a specified value `n0`, given a Poisson distribution with an expected number of events `n0`.
 *
 * @param n The number of events observed.
 * @param n0 The expected number of events.
 *
 * @return The probability that `n` or more events are observed given a Poisson distribution with `n0` expected events.
 */
double poissonSigLevel(long n, double n0)
{
  double sum, term, result = 0;
  long i;
  if (n == 0)
    return 1;
  if (n < 0 || n0 <= 0) {
    return n0 < n ? 0 : 1;
  }
  if (n0 > 200) {
    result = normSigLevel((n - n0) / sqrt(n0), 1);
    if (n < n0)
      return 1 - result;
    return result;
  }
  if (!exp(-n0))
    return n0 < n ? 0 : 1;
  sum = 1;
  term = 1;
  i = 0;
  while (++i < n) {
    term *= n0 / i;
    sum += term;
  }
  result = 1 - sum * exp(-n0);
  if (isnan(result)) {
    return n0 < n ? 0 : 1;
  }
  if (result < 0)
    return 0;
  return result;
}
