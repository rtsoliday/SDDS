/**
 * @file k23.c
 * @brief Provides a function to compute the Modified Bessel Function of the Second Kind K_{2/3}(z).
 *
 * This file implements k23(), which calculates the Modified Bessel Function K_{2/3}(z)
 * over a range of input values. It uses a series expansion for smaller arguments and an
 * asymptotic expansion for larger arguments, referencing Abramowitz & Stegun and based on
 * Roger Dejus's original Fortran code k23.f.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author R. Dejus, H. Shang, R. Soliday
 */
#include "mdb.h"
#include "constants.h"
#include <math.h>
#define A_LIM 10.1
#define EPS1 1.0e-12
#define EPS2 1.0e-8
#define GAMMA_OF_NY 1.354117939426400463
#define NY 2.0 / 3.0

/**
 * @brief Compute the Modified Bessel Function of the Second Kind K_{2/3}(z).
 *
 * This function calculates K_{2/3}(z) for inputs approximately in the range 0.0 < z < 60.0.
 * For z < A_LIM, it applies a series expansion (Abramowitz & Stegun Eq. 9.6.2 and 9.6.10).
 * For z ≥ A_LIM, it uses an asymptotic expansion (Eq. 9.7.2). The method is adapted from
 * Roger Dejus’s code, taking into account precomputed gamma function values and 
 * ensuring numerical stability within specified tolerances (EPS1 and EPS2).
 *
 * @param z The input value for which K_{2/3}(z) is evaluated.
 * @return The computed K_{2/3}(z).
 */
double k23(double z) {
  double c1, c2, zs, ny_fac, neg_ny_fac, zm, zp, pm, pp, term, sum, ze, za, mu, pa;
  static double e2;
  long k;

  if (z < A_LIM) {
    /* series expansion*/
    c1 = PI / 2.0 / sin(PI * NY);
    c2 = 2 * c1;
    zs = z * z / 4.0;

    ny_fac = NY * GAMMA_OF_NY;     /* ! Factorial of (+ny)*/
    neg_ny_fac = c2 / GAMMA_OF_NY; /*! Factorial of (-ny)*/
    e2 = pow(z / 2.0, NY);
    zm = 1 / e2 / neg_ny_fac;
    zp = e2 / ny_fac;
    /* first term */
    pm = pp = 1.0;
    sum = term = c1 * (pm * zm - pp * zp);
    k = 0;
    while (fabs(term) > EPS1 * sum) {
      k = k + 1;
      pm = pm * zs / (k * (k - NY));
      pp = pp * zs / (k * (k + NY));
      term = c1 * (pm * zm - pp * zp);
      sum += term;
    }
  } else {
    ze = sqrt(PI / 2.0 / z) * exp(-z);
    za = 1.0 / (8.0 * z);
    mu = 4.0 * NY * NY;
    /* first term */
    pa = 1.0;
    sum = term = pa * ze;
    /* Additional terms */
    k = 0;
    while (fabs(term) > EPS2 * sum) {
      k = k + 1;
      pa = pa * za * (mu - (2 * k - 1) * (2 * k - 1)) / k;
      term = pa * ze;
      sum += term;
    }
  }
  return sum;
}
