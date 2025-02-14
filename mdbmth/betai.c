/**
 * @file betai.c
 * @brief Routines for computing the incomplete beta function.
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

#include "mdb.h"

#if defined(_WIN32)
#  if _MSC_VER <= 1600
#    include "fdlibm.h"
#  endif
#endif

double betaIncSum(double x, double a, double b);
double betaInc1(double x, double a, double b);

#define BETAI_ACCURACY 1e-10

/**
 * @brief Compute the complete beta function component.
 *
 * Uses the gamma function to compute the value of Beta(a,b) = Γ(a)*Γ(b)/Γ(a+b).
 *
 * @param a First parameter.
 * @param b Second parameter.
 * @return The value of Beta(a,b).
 */
double betaComp(double a, double b) {
#if defined(vxWorks)
  fprintf(stderr, "lgamma function not implemented on vxWorks\n");
  exit(1);
#else
  return exp(lgamma(a) + lgamma(b) - lgamma(a + b));
#endif
}

double lnBetaComp(double a, double b) {
#if defined(vxWorks)
  fprintf(stderr, "lgamma function not implemented on vxWorks\n");
  exit(1);
#else
  return lgamma(a) + lgamma(b) - lgamma(a + b);
#endif
}

/**
 * @brief Compute the incomplete beta function.
 *
 * Calculates the incomplete beta function I_x(a,b) for given parameters a, b and x.
 * If necessary, parameters may be swapped internally to improve convergence.
 *
 * @param a First parameter (must be > 0).
 * @param b Second parameter (must be > 0).
 * @param x The integration limit (0 ≤ x ≤ 1).
 * @return The value of I_x(a,b), or -1.0 if x is out of range.
 */
double betaInc(double a, double b, double x) {
  double xLimit, sum, lnBab = 0;
  short swapped = 0;

  if (x < 0 || x > 1)
    return -1.0;
  if (a + b != 2) {
    xLimit = (a + 1) / (a + b - 2);
    if (x > xLimit) {
      swapped = 1;
      x = 1 - x;
      SWAP_DOUBLE(a, b);
    }
  }
  lnBab = lnBetaComp(a, b);
  /*    sum = pow(x, a)*pow(1-x, b)*betaIncSum(a, b, x)/(Bab*a); */
  sum = exp(a * log(x) + b * log(1 - x) - lnBab) * betaIncSum(a, b, x) / a;
  if (!swapped)
    return sum;
  return 1 - sum;
}

#define MAXIMUM_ITERATIONS 10000

double betaIncSum(double a, double b, double x) {
  double f1, f2;
  double A0, B0, A_1, B_1, A1, B1, d;
  double m, aM1, aP1, aPb, mT2;

  /* evaluate the first step (d1) outside the loop to get started */
  A_1 = B_1 = A0 = 1;
  B0 = 1 - (a + b) / (a + 1) * x;
  aPb = a + b;
  aP1 = a + 1;
  aM1 = a - 1;
  m = 1;
  do {
    mT2 = m * 2;
    d = m * (b - m) * x / ((aM1 + mT2) * (a + mT2));
    A1 = A0 + d * A_1;
    B1 = B0 + d * B_1;
    f1 = A1 / B1;
    A_1 = A0;
    B_1 = B0;
    A0 = A1;
    B0 = B1;
    d = -(a + m) * (aPb + m) * x / ((a + mT2) * (aP1 + mT2));
    A1 = A0 + d * A_1;
    B1 = B0 + d * B_1;
    f2 = A1 / B1;
    A_1 = A0;
    B_1 = B0;
    A0 = A1;
    B0 = B1;
    if (B1) {
      /* rescale the recurrence to avoid over/underflow */
      A_1 /= B1;
      B_1 /= B1;
      A0 /= B1;
      B0 = 1;
    }
    m++;
  } while (fabs(f1 - f2) > BETAI_ACCURACY && m < MAXIMUM_ITERATIONS);
  /*
    if (m>=MAXIMUM_ITERATIONS)
        fprintf(stderr, "warning: too many iterations for incomplete beta function sum (%e, %e, %e) (betaInc)\n",
                a, b, x);
*/
  return f2;
}

/* This isn't used, as it isn't as efficient as betaInc(), but it sure is simple! */
double betaInc1(double a, double b, double x) {
  double xp, sum, term;
  long i;
  xp = x;
  i = sum = 0;
  do {
    sum += (term = betaComp(a + 1, i + 1.0) / betaComp(a + b, i + 1.0) * xp);
    xp = x * xp;
    i++;
  } while (term > BETAI_ACCURACY);
  return (sum + 1) * pow(x, a) * pow(1 - x, b) / (a * betaComp(a, b));
}
